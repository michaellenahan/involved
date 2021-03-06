<?php

/**
 * @file
 * Re-populate group ID and group entity, deprecating the OG group entity.
 *
 * Class should be included only if this is an upgrade from branch 7.x-1.x
 * to branch 7.x-2.x
 */

if (db_field_exists('og_membership', 'group_type') && db_table_exists('og') && !db_table_exists('d6_og')) {

class OgMigrateMembership extends OgEntityMigration {

  /**
   * The name of the table migrate will query.
   */
  public $tableName = 'og_membership';

  /**
   * Indicate we are updating existing data.
   */
  protected $systemOfRecord = Migration::DESTINATION;

  public function __construct() {
    $this->description = t('Re-populate group ID and group entity, deprecating the OG group entity.');

    $query = db_select('og_membership', 'ogm');
    $query->innerJoin('og', 'og', 'ogm.gid = og.gid OR (ogm.group_type = og.entity_type AND ogm.gid = og.etid)');
    $query
      ->fields('ogm', array('id', 'etid', 'entity_type', 'field_name'));

    $query->addField('og', 'etid', 'gid');
    $query->addField('og', 'entity_type', 'group_type');

    $this->query = $query;
    parent::__construct();

    $fields = array(
      'id',
      'group_type',
      'gid',
      'entity_type',
      'etid',
      'field_name',
    );

    foreach ($fields as $field) {
      $this->addFieldMapping($field, $field);
    }
  }

  public function prepare($entity, $row) {
    $entity_type = $row->entity_type;
    $etid = $row->etid;
    $group_type = $row->group_type;
    $gid = $row->gid;

    if (!$group_content = entity_load_single($entity_type, $etid)) {
      // The OG membership was somehow not deleted when the entity
      // was deleted.
      return;
    }

    if (!$group = entity_load_single($group_type, $gid)) {
      return;
    }

    list(,, $group_bundle) = entity_extract_ids($group_type, $group);
    if (!$field_name = og_get_best_group_audience_field($entity_type, $group_content, $group_type, $group_bundle)) {
      // Create a new field. Pick an unused name.
      $field_name = substr("og_$group_type", 0, 32);
      $i = 1;
      while (field_info_field($field_name)) {
        $field_name = substr("og_$group_type", 0, 32 - strlen($i)) . $i;
        ++$i;
      }

      $og_field = og_fields_info(OG_AUDIENCE_FIELD);
      $og_field['field']['settings']['target_type'] = $group_type;

      list(,, $bundle) = entity_extract_ids($entity_type, $group_content);
      og_create_field($field_name, $entity_type, $bundle, $og_field);
    }
    $entity->field_name = $field_name;
  }

  /**
   * Override Migration::postImport().
   *
   * Remove OG-memberships that should hae been deleted.
   *
   */
  protected function postImport() {
    if (!$this->isComplete()) {
      return;
    }
    db_delete('og_membership')
      ->condition('group_type', '')
      ->execute();
  }
}

}
