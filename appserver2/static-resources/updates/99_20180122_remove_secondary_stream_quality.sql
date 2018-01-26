-- Rename vms_camera_user_attributes::secondary_quality to disable_dual_streaming (fix a typo).

CREATE TABLE "vms_scheduletask_tmp" (
    id integer PRIMARY KEY AUTOINCREMENT,
    camera_attrs_id integer NOT NULL,
    start_time integer NOT NULL,
    end_time integer NOT NULL,
    do_record_audio bool NOT NULL,
    record_type smallint NOT NULL,
    day_of_week smallint NOT NULL,
    before_threshold integer NOT NULL,
    after_threshold integer NOT NULL,
    stream_quality smallint NOT NULL,
    fps integer NOT NULL,
    bitrate_kbps INTEGER NOT NULL default 0
);

INSERT INTO vms_scheduletask_tmp
    SELECT * FROM vms_scheduletask;


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
    license_used bool,
    failover_priority integer NOT NULL DEFAULT 2,
    backup_type integer NOT NULL DEFAULT 4
);

INSERT INTO "vms_camera_user_attributes"
    SELECT * FROM "vms_camera_user_attributes_tmp";

-- secondary_quality 4 means 'SSQualityDontUse'
UPDATE vms_camera_user_attributes
   SET disable_dual_streaming = CASE(
    SELECT secondary_quality 
      FROM vms_camera_user_attributes_tmp old 
     WHERE old.id = vms_camera_user_attributes.id)
 WHEN 4 THEN 1 ELSE 0 END;

DROP TABLE "vms_camera_user_attributes_tmp";

-- Rebuild vms_scheduletask after rebuilding vms_camera_user_attributes because of the foreign key.

DROP TABLE "vms_scheduletask";

CREATE TABLE "vms_scheduletask" (
    id integer PRIMARY KEY AUTOINCREMENT,
    camera_attrs_id integer NOT NULL,
    start_time integer NOT NULL,
    end_time integer NOT NULL,
    do_record_audio bool NOT NULL,
    record_type smallint NOT NULL,
    day_of_week smallint NOT NULL,
    before_threshold integer NOT NULL,
    after_threshold integer NOT NULL,
    stream_quality smallint NOT NULL,
    fps integer NOT NULL,
    bitrate_kbps INTEGER NOT NULL default 0,
    FOREIGN KEY(camera_attrs_id) REFERENCES "vms_camera_user_attributes"(id) ON DELETE CASCADE ON UPDATE CASCADE
);

INSERT INTO vms_scheduletask
    SELECT * FROM vms_scheduletask_tmp;

DROP TABLE vms_scheduletask_tmp;
