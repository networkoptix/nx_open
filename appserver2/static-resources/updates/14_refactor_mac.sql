update vms_camera set physical_id = coalesce( 
	(select kv.value 
	  from vms_resource r
               join vms_kvpair kv on kv.resource_guid = r.guid
         where r.id = vms_camera.resource_ptr_id and kv.name = 'DeviceID')
, physical_id);
