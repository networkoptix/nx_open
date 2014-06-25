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
    "redundancy" bool);
INSERT INTO vms_server_tmp
 SELECT api_url, auth_key, version, net_addr_list, resource_ptr_id, panic_mode, 0, '', 0,0
  FROM  vms_server;
drop table  vms_server;
ALTER TABLE vms_server_tmp RENAME TO vms_server;

CREATE TABLE "vms_license_tmp" ("license_block" varchar(2048) NOT NULL, "license_key" varchar(32) NOT NULL UNIQUE, "id" integer PRIMARY KEY autoincrement);
INSERT INTO "vms_license_tmp" (license_key, license_block)
    SELECT key, raw_license FROM vms_license;
DROP TABLE vms_license;
ALTER TABLE "vms_license_tmp" RENAME TO "vms_license";

CREATE TABLE misc_data (key VARCHAR(64), data BLOB(128));
