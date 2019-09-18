-- Delete vms_camera_user_attributes::license_used - it is superceded by schedule_enabled.

ALTER TABLE "vms_camera_user_attributes" RENAME TO "vms_camera_user_attributes_tmp";

CREATE TABLE "vms_camera_user_attributes" (
    id integer PRIMARY KEY AUTOINCREMENT,
    camera_guid BLOB(16) NOT NULL UNIQUE,
    camera_name varchar(200) NOT NULL DEFAULT '',
    audio_enabled bool NOT NULL DEFAULT '0',
    control_enabled bool NOT NULL DEFAULT '0',
    region varchar(1024) NOT NULL DEFAULT '',
    schedule_enabled bool NOT NULL DEFAULT '0',
    motion_type smallint NOT NULL DEFAULT '0',
    disable_dual_streaming smallint NOT NULL DEFAULT '0',
    dewarping_params varchar(200),
    min_archive_days number,
    max_archive_days number,
    preferred_server_id BLOB(16) ,
    group_name varchar(200) NOT NULL DEFAULT '',
    failover_priority integer NOT NULL DEFAULT 2,
    backup_type integer NOT NULL DEFAULT 4,
    logical_id varchar(200),
    record_before_motion_sec integer,
    record_after_motion_sec integer
);

INSERT INTO "vms_camera_user_attributes"
    SELECT
        id,
        camera_guid,
        camera_name,
        audio_enabled,
        control_enabled,
        region,
        schedule_enabled,
        motion_type,
        disable_dual_streaming,
        dewarping_params,
        min_archive_days,
        max_archive_days,
        preferred_server_id,
        group_name,
        failover_priority,
        backup_type,
        logical_id,
        record_before_motion_sec,
        record_after_motion_sec
    FROM "vms_camera_user_attributes_tmp";

DROP TABLE "vms_camera_user_attributes_tmp";
