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

ALTER TABLE "vms_server" ADD flags number;
ALTER TABLE "vms_server" ADD max_cameras number;
ALTER TABLE "vms_server" ADD redundancy bool;

CREATE TABLE "vms_license_tmp" ("license_block" varchar(2048) NOT NULL, "license_key" varchar(32) NOT NULL UNIQUE, "id" integer PRIMARY KEY autoincrement);
INSERT INTO "vms_license_tmp" (license_key, license_block)
    SELECT key, raw_license FROM vms_license;
DROP TABLE vms_license;
ALTER TABLE "vms_license_tmp" RENAME TO "vms_license";

