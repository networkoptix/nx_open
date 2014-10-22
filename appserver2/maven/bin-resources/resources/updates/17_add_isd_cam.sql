INSERT INTO vms_resourcetype (name, manufacture_id) VALUES('XMS-AF-3M', 10);
INSERT INTO vms_resourcetype_parents (from_resourcetype_id, to_resourcetype_id) VALUES(
  (select ID from vms_resourcetype where name = 'XMS-AF-3M'), (select ID from vms_resourcetype where name = 'ISDcam') );

INSERT INTO vms_resourcetype (name, manufacture_id) VALUES('JMS-AF-1080P', 10);
INSERT INTO vms_resourcetype_parents (from_resourcetype_id, to_resourcetype_id) VALUES(
  (select ID from vms_resourcetype where name = 'JMS-AF-1080P'), (select ID from vms_resourcetype where name = 'ISDcam') );
