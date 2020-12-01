DELETE FROM vms_kvpair WHERE resource_guid NOT IN (SELECT guid FROM vms_resource);
