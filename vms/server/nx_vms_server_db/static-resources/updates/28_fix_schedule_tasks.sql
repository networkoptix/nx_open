ALTER TABLE "vms_scheduletask" RENAME TO "vms_scheduletask_tmp";
CREATE TABLE "vms_scheduletask" (
    id                  integer     PRIMARY KEY AUTOINCREMENT,
    camera_attrs_id     integer     NOT NULL,
    start_time          integer     NOT NULL,
    end_time            integer     NOT NULL,
    do_record_audio     bool        NOT NULL,
    record_type         smallint    NOT NULL,
    day_of_week         smallint    NOT NULL,
    before_threshold    integer     NOT NULL,
    after_threshold     integer     NOT NULL,
    stream_quality      smallint    NOT NULL,
    fps                 integer     NOT NULL,
    FOREIGN KEY(camera_attrs_id)    REFERENCES vms_camera_user_attributes(id) ON DELETE CASCADE ON UPDATE CASCADE
);

DELETE FROM vms_scheduletask_tmp WHERE NOT EXISTS (SELECT 1 FROM vms_camera_user_attributes WHERE vms_camera_user_attributes.id = camera_attrs_id);

INSERT INTO vms_scheduletask
   SELECT
    id,
    camera_attrs_id,
    start_time,
    end_time,
    do_record_audio,
    record_type,
    day_of_week,
    before_threshold,
    after_threshold,
    stream_quality,
    fps
  FROM vms_scheduletask_tmp;

DROP TABLE vms_scheduletask_tmp;
