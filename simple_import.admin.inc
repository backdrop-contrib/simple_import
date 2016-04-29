<?php

/**
 * Create the current step of the import assitant.
 */
function simple_import_import_data_form($form, &$form_state, $entity_type) {
  // Initialize the form's steps if it is not already set.
  if (empty($form_state['storage']['current_step'])) {
    $form_state['storage']['current_step'] = 'import_source';
    $form_state['storage']['steps'] = array(
      'import_source' => array(
        'submit' => 'import_source_setup',
        'back' => 'import_source',
        'form' => 'simple_import_source_file_form',
      ),
      'import_source_setup' => array(
        'submit' => 'field_mapping',
        'back' => 'import_source',
        'form' => 'simple_import_import_source_setup_form',
      ),
      'field_mapping' => array(
        'submit' => 'confirmation',
        'back' => 'import_source_setup',
        'form' => 'simple_import_import_data_field_mapping_form',
      ),
      'confirmation' => array(
        'back' => 'field_mapping',
        'form' => 'simple_import_import_data_confirmation_form',
      ),
    );
    $form_state['storage']['entity_type'] = $entity_type;
  }

  $current_step = $form_state['storage']['current_step'];
  $steps = $form_state['storage']['steps'];

  // Build the form for the current step
  $form = $steps[$current_step]['form']($form, $form_state);

  // Add global validation and submission handlers to advance the form step
  $form['#validate'][] = 'simple_import_import_nodes_form_validate';
  $form['#submit'][] = 'simple_import_import_nodes_form_submit';

  // Add a "Back" button on all but the first step
  if ($form_state['storage']['current_step'] != 'import_source') {
    $form['back'] = array(
      '#type' => 'button',
      '#value' => t('Back'),
    );
  }
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );

  return $form;
}

/**
 * Verifiy the form has a valid step.
 */
function simple_import_import_nodes_form_validate(&$form, &$form_state) {
  $form_steps = array_keys($form_state['storage']['steps']);

  if (!in_array($form_state['storage']['current_step'], $form_steps)) {
    form_set_error('', t('Invalid form state'));
  }

  if (!empty($form_state['clicked_button']) && $form_state['clicked_button']['#id'] == 'edit-back') {
    $current_step = $form_state['storage']['current_step'];
    $form_state['storage']['current_step'] = $form_state['storage']['steps'][$current_step]['back'];
    $form_state['rebuild'] = TRUE;
  }
}

/**
 * Advance the form to the next step.
 */
function simple_import_import_nodes_form_submit($form, &$form_state) {
  // Advance if they clicked submit, back up if they clicked 'Back'
  $current_step = $form_state['storage']['current_step'];
  if (!empty($form_state['storage']['steps'][$current_step]['submit'])) {
    $form_state['storage']['current_step'] = $form_state['storage']['steps'][$current_step]['submit'];
    $form_state['rebuild'] = TRUE;
  }
}

/*
 * Default form when no configuration is required by the importer.
 */
function simple_import_no_configuration_form($form, &$form_state) {
  $form['notice'] = array(
    '#type' => 'markup',
    '#markup' => t('No configuration is needed for this type of import.'),
    '#prefix' => '<div class="instructions">',
    '#suffix' => '</div>',
  );

  return $form;
}

/*
 * STEP 1: The input source form.
 */
function simple_import_source_file_form($form, &$form_state) {
  $max_size = file_upload_max_size();

  $format_handlers = simple_import_get_handlers();

  // Valid extensions are the combined list of extensions of all handlers
  $valid_extensions = implode(' ', array_reduce(
    array_column($format_handlers, 'suffixes'), 'array_merge', array()
  ));

  $form['instructions'] = array(
    '#type' => 'markup',
    '#markup' => t('Choose one of the following methods for importing data.  After submitting the data, the server will try to parse the data and you will be able to map the fields in the file to fields of nodes on the system.'),
    '#prefix' => '<div class="instructions">',
    '#suffix' => '</div>',
  );

  $form['file_upload'] = array(
    '#type' => 'file',
    '#title' => t('Upload a file'),
    '#description' => t('Upload a file in one of the supported formats.  Your webserver is set to accept file uploads up to @max_size. Supported formats are %formats.', array('@max_size' => format_size($max_size), '%formats' => $valid_extensions)),
  );

  $form['file_url'] = array(
    '#type' => 'textfield',
    '#title' => t('URL to data'),
    '#description' => t('Or provide a URL to a file we can download.'),
  );

  $form['file_path'] = array(
    '#type' => 'textfield',
    '#title' => t('File on local server'),
    '#description' => t('Or, if you have uploaded the file to the web server, provide the local full path to the file.  It must be readable by the webserver.'),
  );

  $form['#validate'][] ='simple_import_source_file_form_validate';
  $form['#submit'][] = 'simple_import_import_source_form_submit';

  return $form;
}

function simple_import_source_file_form_validate($form, &$form_state) {
  $format_handlers = simple_import_get_handlers();

  // Valid extensions are the combined list of extensions of all handlers
  $valid_extensions = implode(' ', array_reduce(
    array_column($format_handlers, 'suffixes'), 'array_merge', array()
  ));
  $file = file_save_upload('file_upload', array(
    'file_validate_extensions' => $valid_extensions,
  ));
  // Error on save?
  if ($file === FALSE) {
    form_set_error('file_upload', 'Could not save file.');
  }

  // If a file appeared
  if ($file) {
    $file_handler = simple_import_identify_file_type($file->uri, $form_state);
    if (!$file_handler) {
      form_set_error('file_upload', t('Could not identify file type.'));
    }
    $form_state['storage']['file'] = $file;
    $form_state['storage']['file_handler'] = $file_handler;
  }

  if (!$file) {
    form_set_error('', t('No file to import.'));
  }
}

function simple_import_import_source_form_submit($form, &$form_state) {
  $format_handlers = simple_import_get_handlers();
  // Process the data in the file to discover the fields available.
}

/*
 * STEP 2: Configure import source handler
 */
function simple_import_import_source_setup_form($form, &$form_state) {
  $format_handlers = simple_import_get_handlers();

  $handler = $format_handlers[$form_state['storage']['file_handler']];
  $format_setup_form = empty($handler['configure form']) ?
    'simple_import_no_configuration_form' :
    $handler['configure form'];

  return $format_setup_form($form, $form_state);
}

/*
 * STEP 3: The field mapping
 */

function simple_import_import_data_field_mapping_form($form, &$form_state) {
  $type_names = node_type_get_names();
  $types = node_type_get_types();
  $source_fields = $form_state['storage']['source_field_list'];

  $form_state['storage'] = array_merge($form_state['storage'], $form_state['values']);

  // Set form defaults
  $form_state['storage'] += array(
    'node_type' => NULL,
    'field_map' => array(),
  );

  if (!empty($form_state['storage']['node_type'])) {
    $fields = field_info_instances('node', $form_state['storage']['node_type']);
    $form_state['storage']['field_map'] += array_fill_keys(array_keys($fields) , '');
  }

  $data = &$form_state['storage'];

  $form['instructions'] = array(
    '#type' => 'markup',
    '#markup' => t('Map fields below'),
    '#prefix' => '<div class="instructions">',
    '#suffix' => '</div>',
  );

  $form['node_type'] = array(
    '#type' => 'select',
    '#title' => t('Node Types'),
    '#options' => array_merge(array('' => t(' - Select - ')), $type_names),
    '#default_value' => $data['node_type'],
    '#description' => t('Choose the node type to map.  The fields will appear below.  A field set to "Raw Record" will get the original record from the import file, "Static Text" sets the same value for every record, and "Field Default" will apply the default for the field.  If unsure, Field Default is a safe choice.'),
    '#ajax' => array(
      'callback' => 'simple_import_node_type_callback',
      'wrapper' => 'simple-import-field-map',
      'method' => 'replace',
      'event' => 'change',
    ),
  );

  $form['field_map'] = array(
    '#type' => 'item',
    '#id' => 'simple-import-field-map',
    '#tree' => TRUE,
  );

  $field_options = array(
    '' => t(' - Select - '),
    '#static' => t(' - Static Text - '),
    '#raw_record' => t(' - Raw Record - '),
  );
  $field_options += $source_fields;

  $form['field_map']['_title'] = array(
    '#type' => 'select',
    '#title' => t('Title'),
    '#options' => $field_options,
    '#default_value' => $data['field_map']['_title'],
    '#required' => TRUE,
  );

  $field_options = array(
    '#field_default' => t(' - Field Default -'),
    '#static' => t(' - Static Text - '),
    '#raw_record' => t(' - Raw Record - '),
  );
  $field_options += $source_fields;

  if ($data['node_type']) {
    foreach ($fields as $field_name => $info) {
      $field_info = field_info_field($field_name);
      $field_type_info = field_info_field_types($field_info['type']);
      $field_link = l($info['field_name'], sprintf('admin/structure/types/manage/%s/fields/%s', $info['bundle'], $info['field_name']), array('attributes' => array('target'=>'_blank')));
      $description = '<div class="field-details">' .
        t('Machine Name: !field_link / Type: @type', array(
          '!field_link' => $field_link, '@type' => $info['widget']['type']
          )) . '</div>';
      foreach ($field_info['columns'] as $column_name => $column_info) {
        $title = $info['label'];
        if (count($field_info['columns']) > 1) {
          $title .= ':' . $column_name;
        }
        $form['field_map'][$field_name][$column_name] = array(
          '#type' => 'select',
          '#title' => $title,
          '#options' => $field_options,
          '#default_value' => $data['field_map'][$field_name],
          '#description' => $description . $info['description'],
          '#required' => $info['required'],
        );
      }
    }
  }

  $form['#submit'][] = 'simple_import_import_data_field_mapping_submit';

  return $form;
}

function simple_import_node_type_callback($form, &$form_state) {
  return $form['field_map'];
}

function simple_import_import_data_field_mapping_submit($form, &$form_state) {
  $form_state['storage']['field_map'] = $form_state['values']['field_map'];
}

function simple_import_identify_file_type($uri, &$form_state) {
  $format_handlers = simple_import_get_handlers();
  $pathinfo = pathinfo($uri);
  foreach($format_handlers as $type => $handler) {
    if (in_array($pathinfo['extension'], $handler['suffixes'])) {
      // Load the handler file
      simple_import_load_handler($type, $form_state);
      // Try the identify handler
      $callback = $handler['identify callback'];
      if ($callback($uri, $form_state)) {
        return $type;
      }
    }
  }

  return FALSE;
}

/*
 * STEP 4: Confirmation and batch processing.
 */
function simple_import_import_data_confirmation_form($form, &$form_state) {
  $form['#submit'][] = 'simple_import_import_data_confirmation_submit';

  return $form;
}

function simple_import_import_data_confirmation_submit(&$form, &$form_state) {
  $format_handlers = simple_import_get_handlers();

  $handler = $format_handlers[$form_state['storage']['file_handler']];
  $batch = array(
    'title' => t('Importing from %'),
    'operations' => array(
      array($handler['process callback'], array('settings' => $form_state['storage'])),
    ),
    'finished' => 'simple_import_batch_completed',
  );
  if ($handler['file']) {
    $batch['file'] = backdrop_get_path('module', $handler['module']) . '/' . $handler['file'];
  }
  batch_set($batch);
}


function simple_import_batch_completed($success, $results, $operations) {
  if ($success) {
    drupal_set_message(t('Converted @count records.', array('@count' => $state['results']['total_count'])));
  }
  else {
    drupal_set_message(t('Converted @count records before encoutering an error.', array('@count' => $state['results']['total_count'])));
  }
}
