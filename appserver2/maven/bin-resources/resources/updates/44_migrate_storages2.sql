UPDATE vms_resource
SET url = replace( url, '@/%5C%5C', '@' ) 
WHERE id in (SELECT resource_ptr_id from vms_storage);

UPDATE vms_resource
SET url = replace( url, 'smb:///%5C%5C', 'smb://' ) 
WHERE id in (SELECT resource_ptr_id from vms_storage);
