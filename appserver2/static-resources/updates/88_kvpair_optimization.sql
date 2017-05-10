DROP INDEX idx_kvpair_name;
CREATE  INDEX idx_kvpair ON vms_kvpair (resource_guid);
