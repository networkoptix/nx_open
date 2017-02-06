
ALTER TABLE vms_camera_user_attributes ADD COLUMN group_name varchar(200) NOT NULL DEFAULT '';

UPDATE vms_camera_user_attributes SET group_name = 
    (SELECT c.group_name
     FROM vms_camera as c, vms_resource as r
     WHERE r.guid=vms_camera_user_attributes.camera_guid
       AND c.resource_ptr_id = r.id)
  WHERE EXISTS (SELECT * FROM vms_resource as r WHERE r.guid=vms_camera_user_attributes.camera_guid);
