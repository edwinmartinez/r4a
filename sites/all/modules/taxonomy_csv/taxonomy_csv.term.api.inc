<?php

/**
 * @file
 * Find, get and set full or detail term items.
 */

/**
 * Find a term by its name and load it. This function manages duplicates.
 *
 * If the term has got duplicates, only first one (lower tid) will be returned.
 *
 * @note
 * Need to maintain a specific function and a direct query, because
 * taxonomy_term_load_multiple doesn't manage parents and duplicates.
 * db_query() is prefered, because it's four times faster than db_select(),
 * EntityFieldQuery and taxonomy_get_term_by_name() (these last two don't manage
 * parents'). Anyway terms are loaded to get all fields, specialy parents.
 *
 * @param $term
 *   The term object to find. It's not necessarily a standard term object. It's
 *   an object which needs only a name and eventually a vid, a parent id and a
 *   language. Of course, if tid is set, the found term is the existing one.
 * @param $all_vocabularies
 *   (Optional) Boolean. Search in all vocabularies or only in $term->vid
 *   vocabulary (default), which need to be set. Used with relations import.
 * @param $parent_tid
 *   (Optional) The direct parent term id where to restrict search.
 *   Used for structure import. Default to NULL (no parent restriction).
 *
 * @return
 *   Formatted found term object, or FALSE if not found or error.
 */
function taxonomy_csv_term_find($term, $all_vocabularies = FALSE, $parent_tid = NULL) {
  if (isset($term->tid) && $term->tid) {
    return taxonomy_term_load($term->tid);
  }

  static $flag_i18n = NULL;
  if (is_NULL($flag_i18n)) {
    $flag_i18n = module_exists('i18n_taxonomy');
  }

  if (isset($term->name)) {
    $name = drupal_strtolower(trim($term->name));

    // Only term id is selected, because taxonomy_term_load is used next in
    // order to take advantage of taxonomy cache.
    $sql = '
      SELECT t.tid
      FROM {taxonomy_term_data} t
      INNER JOIN {taxonomy_term_hierarchy} h ON t.tid = h.tid
      WHERE :name LIKE LOWER(t.name)
    ';
    $args = array();
    $args[':name'] = $name;

    if (isset($term->vid)
        && $term->vid
        && !$all_vocabularies) {
      $sql .= ' AND t.vid = :vid';
      $args[':vid'] = $term->vid;
    }

    if ($flag_i18n && isset($term->language)) {
      $sql .= ' AND t.language = :language';
      $args[':language'] = $term->language;
    }

    if ($parent_tid) {
      $sql .= ' AND h.parent = :parent';
      $args[':parent'] = $parent_tid;
    }

    $sql .= ' ORDER BY t.tid ASC LIMIT 1';

    $result = db_query($sql, $args)->fetchField();
    if ($result) {
      return taxonomy_term_load($result);
    }
  }

  // Not found or error.
  return FALSE;
}

/**
 * Find duplicate terms in a vocabulary or in all vocabularies.
 *
 * @todo use taxonomy_term_load_multiple or regular Drupal 7 query.
 *
 * @param $vid
 *  (Optional) Vocabulary to check in.
 *
 * @return
 *  An array of term names, indexed by tid.
 */
function taxonomy_csv_term_find_duplicate($vid = 0) {
  $terms = array();

  $sql = '
    SELECT t1.tid, t1.name
    FROM {taxonomy_term_data} t1
    LEFT OUTER JOIN {taxonomy_term_data} t2 ON t1.tid != t2.tid AND LOWER(t1.name) = LOWER(t2.name)
    WHERE t2.tid IS NOT NULL
  ';

  $args = array();

  if ($vid) {
    $sql .= ' AND t1.vid = :vid AND t2.vid = :vid ';
    $args[':vid'] = $vid;
  }

  $sql .= ' ORDER BY t1.tid ASC ';

  $result = db_query($sql, $args)->fetchAllKeyed();

  return $result;
}

/**
 * Return an array of all parents term ids of a given term id.
 */
function taxonomy_csv_term_get_parents_tids($tid) {
  return array_keys(taxonomy_get_parents($tid));
}

/**
 * Return an array of all parents term names of a given term id.
 */
function taxonomy_csv_term_get_parents_names($tid) {
  $names = array();
  foreach (taxonomy_get_parents($tid) as $parent) {
    $names[] = $parent->name;
  }
  return $names;
}

/**
 * Return an array of all children term ids of a given term id.
 */
function taxonomy_csv_term_get_children_tids($tid) {
  return array_keys(taxonomy_get_children($tid));
}

/**
 * Return an array of all children term names of a given term id.
 */
function taxonomy_csv_term_get_children_names($tid) {
  $names = array();
  foreach (taxonomy_get_children($tid) as $child) {
    $names[] = $child->name;
  }
  return $names;
}

/**
 * Return an array of values of a field of a given term id or object.
 *
 * @todo To be removed by using of field api.
 *
 * Values can be names or ids depending on field type (text or term reference).
 *
 * @param $term_tid
 *   Term object or term id from which to get values. Use of object is quicker.
 * @param $field_name
 *   Field to get.
 * @param $langcode
 *   (Optional) Language code to use.
 *
 * @return
 *   Result array of items.
 */
function taxonomy_csv_term_get_field_values($term_tid, $field_name, $langcode = 'und') {
  $items = array();

  $term = is_object($term_tid) ? $term_tid : taxonomy_term_load($term_tid);

  $value = ($field_name == 'field_term_relation') ?
    'tid' :
    'value';

  if (isset($term->{$field_name}[$langcode])) {
    foreach ($term->{$field_name}[$langcode] as $delta) {
      $items[] = $delta[$value];
    }
  }

  return $items;
}

/**
 * Return an array of all term objects of a reference field of a given term ID.
 *
 * @param $term_tid
 *   Term object or term id from which to get term objects. Use of object is
 *   quicker.
 * @param $field_name
 *   Term reference field to get.
 *
 * @return
 *   Result array of term objects.
 */
function taxonomy_csv_term_get_field_terms($term_tid, $field_name) {
  $items = array();

  $values = taxonomy_csv_term_get_field_values($term_tid, $field_name);

  foreach ($values as $tid) {
    $items[] = taxonomy_term_load($tid);
  }

  return $items;
}

/**
 * Return an array of all names of a term reference field of a given term ID.
 *
 * @param $term_tid
 *   Term object or term id from which to get names. Use of object is quicker.
 * @param $field_name
 *   Term reference field to get.
 *
 * @return
 *   Result array of items.
 */
function taxonomy_csv_term_get_field_names($term_tid, $field_name) {
  $items = array();

  $values = taxonomy_csv_term_get_field_values($term_tid, $field_name);

  foreach ($values as $tid) {
    $term = taxonomy_term_load($tid);
    $items[] = $term->name;
  }

  return $items;
}

/**
 * Return an array of all synonyms of a given term id or object.
 *
 * A synonym is not a term, so it hasn't a tid, but only a name. It's a text
 * field and not a term reference.
 * Wrapper of taxonomy_csv_term_get_field_values.
 */
function taxonomy_csv_term_get_synonyms($term_tid) {
  return taxonomy_csv_term_get_field_values($term_tid, 'field_term_synonym');
}

/**
 * Return an array of all related term ids of a given term id or object.
 * Wrapper of taxonomy_csv_term_get_field_values.
 */
function taxonomy_csv_term_get_relations($term_tid) {
  return taxonomy_csv_term_get_field_terms($term_tid, 'field_term_relation');
}

/**
 * Return an array of all related term ids of a given term id or object.
 * Wrapper of taxonomy_csv_term_get_field_values.
 */
function taxonomy_csv_term_get_relations_tids($term_tid) {
  return taxonomy_csv_term_get_field_values($term_tid, 'field_term_relation');
}

/**
 * Return an array of all related term names of a given term id or object.
 * Wrapper of taxonomy_csv_term_get_field_names.
 */
function taxonomy_csv_term_get_relations_names($term_tid) {
  return taxonomy_csv_term_get_field_names($term_tid, 'field_term_relation');
}

/**
 * Return the first path to the root of a term.
 *
 * @note
 *   Drupal and taxonomy_csv use 'parent' property, but taxonomy_get_tree() uses
 *   'parents'.
 *
 * @param $term
 *   A term object with 'parent' property.
 * @param $tree
 *   A tree array as obtained with taxonomy_get_tree().
 *
 * @return
 *   Array of term objects matching to the path of a term to its root term.
 *   If a term is a root term, return an empty array.
 */
function taxonomy_csv_term_get_first_path($term, &$tree) {
  $path = array();

  // Items need to be ordered from 0 to get first parent easy.
  if (isset($term->parent)) {
    $term->parent = array_values($term->parent);
  }

  // To use a counter prevents infinite loop when the hierarchy is inconsistent.
  $i = 0;
  while ($i < 100
      // A term root has no parent.
      && isset($term->parent)
      && !empty($term->parent)
      && $term->parent[0] <> 0
    ) {
    $tid = $term->parent[0];
    if ($tid === 0) {
      break;
    }

    // Get the full term from the tree.
    foreach ($tree as $parent) {
      if ($parent->tid == $tid) {
        break;
      }
    }
    if (isset($parent->parents)) {
      $parent->parent = array_values($parent->parents);
      unset($parent->parents);
    }
    $path[] = $term = $parent;

    $i++;
  }

  // The path is reversed in order to begin with root term.
  return array_reverse($path);
}

/**
 * Delete multiple terms.
 *
 * @param $tids
 *   An array of taxonomy term IDs.
 *
 * @return
 *   TRUE.
 */
function taxonomy_csv_term_delete_multiple($tids) {
  if (!is_array($tids)) {
    return FALSE;
  }

  foreach ($tids as $tid) {
    taxonomy_term_delete($tid);
  }
  return TRUE;
}
