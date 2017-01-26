UPDATE vms_resource
SET url = replace(url, '/', '\')
WHERE id in (SELECT resource_ptr_id FROM vms_storage s)
 AND substr(url, 2,1) = ':';
