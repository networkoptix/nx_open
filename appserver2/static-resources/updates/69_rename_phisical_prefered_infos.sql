-- Rename vms_client_infos::phisicalMemory to physicalMemory (fix a typo).
-- Rename vms_client_infos to vms_client_info_list (avoid "infos").

CREATE TABLE "vms_client_info_list" (
    guid BLOB NOT NULL UNIQUE PRIMARY KEY,
    parent_guid BLOB NOT NULL,
    skin TEXT NULL,
    systemInfo TEXT NULL,
    cpuArchitecture TEXT NULL,
    cpuModelName TEXT NULL,
    physicalMemory integer NULL,
    openGLVersion TEXT NULL,
    openGLVendor TEXT NULL,
    openGLRenderer TEXT NULL,
    full_version TEXT,
    systemRuntime TEXT
);

INSERT INTO "vms_client_info_list"
    SELECT * FROM "vms_client_infos";

DROP TABLE "vms_client_infos";

---------------------------------------------------------------------------------------------------
-- Rename vms_camera_user_attributes::prefered_server_id to preferred_server_id (fix a typo).

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
    secondary_quality smallint NOT NULL DEFAULT '1',
    dewarping_params varchar(200),
    min_archive_days number,
    max_archive_days number,
    preferred_server_id BLOB(16) ,
    group_name varchar(200) NOT NULL DEFAULT '',
    license_used bool,
    failover_priority integer NOT NULL DEFAULT 2,
    backup_type integer NOT NULL DEFAULT 4
);

INSERT INTO "vms_camera_user_attributes"
    SELECT * FROM "vms_camera_user_attributes_tmp";

DROP TABLE "vms_camera_user_attributes_tmp";
