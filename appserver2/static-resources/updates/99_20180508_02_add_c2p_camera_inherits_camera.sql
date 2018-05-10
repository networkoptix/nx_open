-- C2pWebPage inherits WebPage

INSERT OR REPLACE INTO "vms_resourcetype_parents" (from_resourcetype_id, to_resourcetype_id) VALUES(
	(SELECT id from vms_resourcetype where name = 'C2pCamera'), 
	(SELECT id from vms_resourcetype where name = 'Camera'));
