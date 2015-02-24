DELETE FROM vms_businessrule_action_resources WHERE businessrule_guid in ( SELECT guid FROM vms_businessrule WHERE vms_businessrule.system = 1);
DELETE FROM vms_businessrule_event_resources WHERE businessrule_guid in ( SELECT guid FROM vms_businessrule WHERE vms_businessrule.system = 1);
DELETE FROM vms_businessrule WHERE vms_businessrule.system = 1;
