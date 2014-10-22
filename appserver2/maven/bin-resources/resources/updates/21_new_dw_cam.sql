INSERT INTO "vms_resourcetype" (name, description, manufacture_id) VALUES('PANORAMIC',NULL,12);
INSERT INTO "vms_resourcetype_parents" (from_resourcetype_id, to_resourcetype_id) VALUES(
	(SELECT id from vms_resourcetype where name = 'PANORAMIC'), 
	(SELECT id from vms_resourcetype where name = 'ONVIF'));
