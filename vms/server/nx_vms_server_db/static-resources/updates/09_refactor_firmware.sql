insert into vms_kvpair (resource_guid, name, value)
select r.guid, 'firmware', c.firmware
  from vms_camera c
  join vms_resource r on r.id = c.resource_ptr_id
 where length(c.firmware) > 0;

ALTER TABLE "vms_camera" RENAME TO "vms_camera_tmp";

CREATE TABLE "vms_camera" (
    "audio_enabled" bool NOT NULL DEFAULT 0,
    "control_enabled" bool NOT NULL DEFAULT 0,
    "vendor" varchar(200) NOT NULL DEFAULT '',
    "manually_added" bool NOT NULL DEFAULT 0,
    "resource_ptr_id" integer PRIMARY KEY autoincrement,
    "region" varchar(1024) NOT NULL,
    "schedule_enabled" bool NOT NULL DEFAULT 0,
    "motion_type" smallint NOT NULL DEFAULT 0,
    "group_name" varchar(200) NOT NULL DEFAULT '',
    "group_id" varchar(200) NOT NULL DEFAULT '',
    "mac" varchar(200) NOT NULL,
    "model" varchar(200) NOT NULL DEFAULT '',
    "secondary_quality" smallint NOT NULL DEFAULT 1,
    "status_flags" integer NOT NULL DEFAULT 0,
    "physical_id" varchar(200) NOT NULL DEFAULT '',
    "password" varchar(200) NOT NULL,
    "login" varchar(200) NOT NULL,
    "dewarping_params" varchar(200));
INSERT INTO "vms_camera" (
    "audio_enabled",
    "control_enabled",
    "vendor",
    "manually_added",
    "resource_ptr_id",
    "region",
    "schedule_enabled",
    "motion_type",
    "group_name",
    "group_id",
    "mac",
    "model",
    "secondary_quality",
    "status_flags",
    "physical_id",
    "password",
    "login",
    "dewarping_params")
SELECT 
    "audio_enabled",
    case when control_disabled > 0 then 0 else 1 end,
    "vendor",
    "manually_added",
    "resource_ptr_id",
    "region",
    case when schedule_disabled > 0 then 0 else 1 end,
    "motion_type",
    "group_name",
    "group_id",
    "mac",
    "model",
    "secondary_quality",
    "status_flags",
    "physical_id",
    "password",
    "login",
    "dewarping_params"
FROM vms_camera_tmp;

drop table vms_camera_tmp;
