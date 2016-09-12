UPDATE vms_resource 
   SET xtype_guid = (SELECT guid from vms_resourcetype where name = 'Digital Watchdog')
 WHERE id in (
  SELECT resource_ptr_id 
    FROM vms_camera
   WHERE vendor = 'Digital Watchdog'
);
