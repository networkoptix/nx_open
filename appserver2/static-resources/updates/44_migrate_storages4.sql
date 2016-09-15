UPDATE vms_resource
SET url = replace( url, '\', '/' ) 
WHERE id in (SELECT resource_ptr_id from vms_storage)
and url like 'smb:%';
