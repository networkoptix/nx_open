-- Migration is processed in the application. Do not rename file.

INSERT INTO "vms_resourcetype" (name, description, manufacture_id) 
     SELECT 'PANORAMIC', NULL, 12
WHERE NOT exists(select 1 from vms_resourcetype where name = 'PANORAMIC');
INSERT OR REPLACE INTO "vms_resourcetype_parents" (from_resourcetype_id, to_resourcetype_id) VALUES(
	(SELECT id from vms_resourcetype where name = 'PANORAMIC'), 
	(SELECT id from vms_resourcetype where name = 'ONVIF'));

INSERT INTO "vms_resourcetype" (name, description, manufacture_id) 
     SELECT 'IPNC', NULL, 12
WHERE NOT exists(select 1 from vms_resourcetype where name = 'IPNC');
INSERT OR REPLACE INTO "vms_resourcetype_parents" (from_resourcetype_id, to_resourcetype_id) VALUES(
	(SELECT id from vms_resourcetype where name = 'IPNC'), 
	(SELECT id from vms_resourcetype where name = 'ONVIF'));
