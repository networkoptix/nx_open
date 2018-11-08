ALTER TABLE vms_camera add prefered_server_id BLOB(16) NULL;
update vms_camera set prefered_server_id = (select guid from vms_resource r where r.id = resource_ptr_id);

