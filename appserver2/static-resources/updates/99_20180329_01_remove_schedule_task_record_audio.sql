-- Remove vms_scheduletask::do_record_audio (unused field).

ALTER TABLE vms_scheduletask RENAME TO vms_scheduletask_tmp;

CREATE TABLE "vms_scheduletask" (
    id integer PRIMARY KEY AUTOINCREMENT,
    camera_attrs_id integer NOT NULL,
    start_time integer NOT NULL,
    end_time integer NOT NULL,
    record_type smallint NOT NULL,
    day_of_week smallint NOT NULL,
    before_threshold integer NOT NULL,
    after_threshold integer NOT NULL,
    stream_quality smallint NOT NULL,
    fps integer NOT NULL,
    bitrate_kbps INTEGER NOT NULL default 0,
    FOREIGN KEY(camera_attrs_id) REFERENCES "vms_camera_user_attributes"(id) ON DELETE CASCADE ON UPDATE CASCADE
);

INSERT INTO vms_scheduletask (
    id, camera_attrs_id, start_time, end_time, record_type,
    day_of_week, before_threshold, after_threshold, stream_quality, fps, bitrate_kbps
    )
    SELECT
    id, camera_attrs_id, start_time, end_time, record_type,
    day_of_week, before_threshold, after_threshold, stream_quality, fps, bitrate_kbps
    FROM vms_scheduletask_tmp;

DROP TABLE vms_scheduletask_tmp;
