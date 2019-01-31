
ALTER TABLE "vms_camera" RENAME TO "vms_camera_tmp";

CREATE TABLE "vms_camera" (
    resource_ptr_id     integer         PRIMARY KEY,
    vendor              varchar(200)    NOT NULL DEFAULT '',
    manually_added      bool            NOT NULL DEFAULT '0',
    group_name          varchar(200)    NOT NULL DEFAULT '',
    group_id            varchar(200)    NOT NULL DEFAULT '',
    mac                 varchar(200)    ,
    model               varchar(200)    NOT NULL DEFAULT '',
    status_flags        integer         NOT NULL DEFAULT '0',
    physical_id         varchar(200)    NOT NULL DEFAULT ''
);

INSERT INTO vms_camera
  SELECT
    resource_ptr_id,
    vendor,
    manually_added,
    group_name,
    group_id,
    mac,
    model,
    status_flags,
    physical_id
  FROM vms_camera_tmp;

DROP TABLE vms_camera_tmp;
