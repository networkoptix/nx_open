update vms_camera set physical_id = coalesce( (select value from vms_kvpair where resource_id = vms_camera.resource_ptr_id and name = 'DeviceID'), physical_id);
