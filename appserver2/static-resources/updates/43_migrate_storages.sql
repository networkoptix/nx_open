UPDATE vms_resource
SET url = replace( url, 'file://', 'smb://' ) WHERE url LIKE 'file://%'
and id in (SELECT resource_ptr_id from vms_storage);
