INSERT INTO "vms_businessrule_action_resources" (businessrule_guid,resource_guid) 
  SELECT br.guid, r.guid
  FROM vms_businessrule_action_resources_tmp t
  JOIN vms_businessrule br on br.id = t.businessrule_id
  JOIN vms_resource r on r.id = t.resource_id;

INSERT INTO "vms_businessrule_event_resources" (businessrule_guid,resource_guid) 
  SELECT br.guid, r.guid
  FROM vms_businessrule_event_resources_tmp t
  JOIN vms_businessrule br on br.id = t.businessrule_id
  JOIN vms_resource r on r.id = t.resource_id;

drop table vms_resource_tmp;
drop table vms_businessrule_action_resources_tmp;
drop table vms_businessrule_event_resources_tmp;
drop table vms_layoutitem_tmp;

CREATE UNIQUE INDEX idx_businessrule_guid ON vms_businessrule(guid);
CREATE UNIQUE INDEX idx_resourcetype_guid ON vms_resourcetype(guid);
CREATE UNIQUE INDEX idx_resource_guid     ON vms_resource(guid);
CREATE INDEX idx_resource_parent          ON vms_resource(parent_guid);

CREATE TABLE "vms_server_tmp" (
    "api_url" varchar(200) NOT NULL,
    "auth_key" varchar(1024),
    "version" varchar(1024),
    "net_addr_list" varchar(1024),
    "resource_ptr_id" integer PRIMARY KEY,
    "panic_mode" smallint NOT NULL,
    "flags" number,
    "system_info" varchar(1024),
    "max_cameras" number,
    "redundancy" bool,
    "system_name" varchar(1024));
INSERT INTO vms_server_tmp
 SELECT api_url, auth_key, version, net_addr_list, resource_ptr_id, panic_mode, 0, '', 0, 0, ''
  FROM  vms_server;
drop table  vms_server;
ALTER TABLE vms_server_tmp RENAME TO vms_server;

CREATE TABLE "vms_license_tmp" ("license_block" varchar(2048) NOT NULL, "license_key" varchar(32) NOT NULL UNIQUE, "id" integer PRIMARY KEY autoincrement);
INSERT INTO "vms_license_tmp" (license_key, license_block)
    SELECT key, raw_license FROM vms_license;
DROP TABLE vms_license;
ALTER TABLE "vms_license_tmp" RENAME TO "vms_license";

CREATE TABLE misc_data (key VARCHAR(64) NOT NULL, data BLOB(128));
CREATE UNIQUE INDEX idx_misc_data_key ON misc_data(key);
drop table vms_setting;

ALTER TABLE vms_propertytype RENAME TO vms_propertytype_tmp;
CREATE TABLE "vms_propertytype" (
    "id" integer NOT NULL PRIMARY KEY,
    "resource_type_id" integer NOT NULL,
    "name" varchar(200) NOT NULL,
    "default_value" varchar(200) NULL);
insert into vms_propertytype select id,	resource_type_id, name, default_value from vms_propertytype_tmp;
drop table vms_propertytype_tmp;

ALTER TABLE vms_kvpair RENAME TO vms_kvpair_tmp;

CREATE TABLE "vms_kvpair" (
    "id" integer PRIMARY KEY autoincrement,
    "resource_guid" BLOB(16) NOT NULL,
    "name" varchar(200) NOT NULL,
    "value" varchar(200) NOT NULL);
insert into vms_kvpair (resource_guid, name, value)
select r.guid, kv.name, kv.value
FROM vms_kvpair_tmp kv
     JOIN vms_resource r on r.id = kv.resource_id;
CREATE UNIQUE INDEX idx_kvpair_name ON vms_kvpair (resource_guid, name);
drop table vms_kvpair_tmp;
