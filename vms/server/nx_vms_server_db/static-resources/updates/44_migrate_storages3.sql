UPDATE vms_resource
SET url = replace( url, '%5C', '/' ) 
WHERE id in (SELECT resource_ptr_id from vms_storage);
