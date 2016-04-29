<?php

function simple_import_csv_identify($uri, &$form_state) {
  $fh = fopen($uri, 'r');
  $csv = fgetcsv($fh);
  fclose($fh);
  return !empty($csv);
}

function simple_import_csv_config_form($form, &$form_state) {

  if (empty($form_state['storage']['csv'])) {
    $form_state['storage']['csv'] = array();
  }
  $current_settings = &$form_state['storage']['csv'];
  if($form_state['clicked_button']['#id'] == 'edit-refresh') {
    $current_settings = array_merge($current_settings, $form_state['values']);
  }
  $current_settings += array(
    'first_line_header' => 0,
    'delimiter' => ",",
    'enclosure' => '"',
    'escape' => '\\',
  );

  // Show a preview of the first two lines of the file
  $fh = fopen($form_state['storage']['file']->uri, 'r');
  $form['first_line'] = array(
    '#caption' => t('First two lines'),
    '#theme' => 'table',
    '#rows' => array(
      array('data' => simple_import_csv_read_next_line($fh, $current_settings)),
      array('data' => simple_import_csv_read_next_line($fh, $current_settings)),
    ),
    '#attributes' => array(
      'class' => array('simple-import-csv-first-line'),
    ),
  );

  $form['first_line_header'] = array(
    '#type' => 'checkbox',
    '#title' => t('First line is header'),
    '#default_value' => $current_settings['first_line_header'],
    '#description' => t('Does the first line of the file contain field headers?'),
  );

  $form['delimiter'] = array(
    '#type' => 'textfield',
    '#title' => t('Delimiter'),
    '#default_value' => $current_settings['delimiter'],
    '#width' => 1,
    '#size' => 1,
    '#description' => t('The character that separates fields in the file.  Defaults to a comma (,)'),
  );
  $form['enclosure'] = array(
    '#type' => 'textfield',
    '#title' => t('Enclosure character'),
    '#default_value' => $current_settings['enclosure'],
    '#width' => 1,
    '#size' => 1,
    '#description' => t('The character that encloses a field.  Delimiters between enclosure characters won\'t trigger a new field.  Ex: <code>"Washington, DC"</code> is one field, but <code>Washington, DC</code> is two.'),
  );
  $form['escape'] = array(
    '#type' => 'textfield',
    '#title' => t('Escape character'),
    '#default_value' => $current_settings['escape'],
    '#width' => 1,
    '#size' => 1,
    '#description' => t('The escape character.  Prevents enclosures from closing prematurely.  Ex. <code>"\\"That\'ll do, pig,\\" he said."</code> is one field because the interior quotes are escaped.  Defaults to backslash (\\).'),
  );

  $form['refresh'] = array(
    '#type' => 'button',
    '#value' => t('Refresh'),
  );

  $form['#submit'][] = 'simple_import_csv_config_form_submit';

  return $form;
}

function simple_import_csv_config_form_submit($form, &$form_state) {
  $form_state['storage']['csv'] = array_merge(
    $form_state['storage']['csv'],
    $form_state['values']
  );

  $fh = fopen($form_state['storage']['file']->uri, 'r');
  $csv_settings = $form_state['storage']['csv'];
  $first_line = simple_import_csv_read_next_line($fh, $csv_settings);

  if ($form_state['values']['first_line_header']) {
    $form_state['storage']['source_field_list'] = $first_line;
  }
  else {
    for($i=1;$i<=count($first_line);$i++) {
      $form_state['storage']['source_field_list']['field_'.$i] = t('Column @c', array('@c' => $i));
    }
  }
}

function simple_import_csv_read_next_line($fh, $csv_settings) {
  return fgetcsv($fh, 0,
    $csv_settings['delimiter'],
    $csv_settings['enclosure'],
    $csv_settings['escape']
  );
}

function simple_import_csv_process($settings, &$context) {
  global $user;

  $fh = fopen($settings['file']->uri, 'r');
  $state = &$context['sandbox'];

  // Initialize
  if (empty($state)) {
    $state['last_pos'] = 0;
    if ($settings['csv']['first_line_header']) {
      $state['headers'] = simple_import_csv_read_next_line($fh, $settings['csv']);
      $state['last_pos'] = ftell($fh);
    }
    $stat = fstat($fh);
    $state['total_size'] = $stat['size'];
    $context['results']['total_count'] = 0;
  }

  // Go back to where we last left off
  fseek($fh, $state['last_pos']);

  // Run for 10 records
  $cycle_count = 0;
  while($cycle_count < 10) {
    $record = simple_import_csv_read_next_line($fh, $settings['csv']);
    if ($record === FALSE) {
      fclose($fh);
      $context['message'] = t('100.00% complete.');
      $context['finished'] = 1;
      return;
    }

    // Create a new node
    $node = $node = entity_create('node', array(
      'type' => $settings['node_type'],
    ));
    node_object_prepare($node);

    $lang = empty($node->langcode) ? LANGUAGE_NONE : $node->langcode;

    foreach ($settings['field_map'] as $field => $mapping) {
      if ($field == '_title') {
        $node->title = simple_import_csv_map_data($record, $mapping, $field, NULL, $settings, $lang);
      }
      else {
        foreach ($mapping as $column => $map_value) {
          $value = simple_import_csv_map_data($record, $mapping, $field, $column, $settings, $lang);
          if ($value !== NULL) {
            $node->{$field}[$lang][0][$column] = $value;
          }
        }
      }
    }

    // Save the node
    node_submit($node);
    $node->save();

    $cycle_count++;
    $context['results']['total_count']++;
  }

  $state['last_pos'] = ftell($fh);

  $pct = 100.0 * $state['last_pos'] / $state['total_size'];
  $context['message'] = t('@pct% complete.', array('@pct' => sprintf('%0.2f', $pct)));
  $context['finished'] = (1.0 * $state['last_pos']) / $state['total_size'];
}

function simple_import_csv_map_data($record, $map, $field, $column, $settings, $lang) {
  $map_value = ($column === NULL) ? $map : $map[$column];
  switch ($map_value) {
    case '#field_default':
      $field_info = field_info_instance($settings['entity_type'], $field, $settings['node_type']);
      $default = field_get_default_value($settings['entity_type'], $settings['node_type'], $field, $field_info, $lang);
      return empty($default[0]) ? NULL : $default[0][$column];

    case '#static':
      return $settings['static'][$field];

    case '#raw_record':
      return implode(',', $record);

    default:
      return $record[$map_value];
  }

  return NULL;
}
