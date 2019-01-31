-- Migration is processed in the application. Do not rename file.

ALTER TABLE "vms_camera" RENAME TO "vms_camera_tmp";

CREATE TABLE "vms_camera" (
    resource_ptr_id     integer         PRIMARY KEY,
    vendor              varchar(200)    NOT NULL DEFAULT '',
    manually_added      bool            NOT NULL DEFAULT '0',
    group_name          varchar(200)    NOT NULL DEFAULT '',
    group_id            varchar(200)    NOT NULL DEFAULT '',
    mac                 varchar(200)    NOT NULL,
    model               varchar(200)    NOT NULL DEFAULT '',
    status_flags        integer         NOT NULL DEFAULT '0',
    physical_id         varchar(200)    NOT NULL DEFAULT ''
);


CREATE TABLE "vms_camera_user_attributes" (
    id                  integer         PRIMARY KEY AUTOINCREMENT,
    camera_guid         BLOB(16)        NOT NULL UNIQUE,
    camera_name         varchar(200)    NOT NULL DEFAULT '',
    audio_enabled       bool            NOT NULL DEFAULT '0',
    control_enabled     bool            NOT NULL DEFAULT '0',
    region              varchar(1024)   NOT NULL DEFAULT '',
    schedule_enabled    bool            NOT NULL DEFAULT '0',
    motion_type         smallint        NOT NULL DEFAULT '0',
    secondary_quality   smallint        NOT NULL DEFAULT '1',
    dewarping_params    varchar(200),
    min_archive_days    number,
    max_archive_days    number,
    prefered_server_id  BLOB(16)
);

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
    FOREIGN KEY(camera_attrs_id)    REFERENCES vms_camera_user_attributes(id)
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

INSERT INTO vms_camera_user_attributes
  SELECT
    c.resource_ptr_id,
    r.guid,
    r.name,
    c.audio_enabled,
    c.control_enabled,
    c.region,
    c.schedule_enabled,
    c.motion_type,
    c.secondary_quality,
    c.dewarping_params,
    c.min_archive_days,
    c.max_archive_days,
    c.prefered_server_id
  FROM vms_camera_tmp c
  JOIN vms_resource r on r.id = c.resource_ptr_id;

INSERT INTO vms_scheduletask
   SELECT
    id,
    source_id,
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

INSERT INTO vms_kvpair (resource_guid, name, value)
     SELECT r.guid, "credentials", c.login || ":" || c.password
      FROM vms_camera_tmp c
      JOIN vms_resource r on r.id = c.resource_ptr_id
     WHERE length(c.login) > 0;


DROP TABLE vms_camera_tmp;

ALTER TABLE "vms_server" RENAME TO "vms_server_tmp";

CREATE TABLE "vms_server" (
    api_url             varchar(200) NOT NULL,
    auth_key            varchar(1024),
    version             varchar(1024),
    net_addr_list       varchar(1024),
    resource_ptr_id     integer,
    panic_mode          smallint NOT NULL,
    flags               number,
    system_info         varchar(1024),
    system_name         varchar(1024),
    PRIMARY KEY(resource_ptr_id)
);



CREATE TABLE "vms_server_user_attributes" (
    id                  integer         PRIMARY KEY AUTOINCREMENT,
    server_guid         BLOB(16)        NOT NULL UNIQUE,
    server_name         varchar(200)    NOT NULL DEFAULT '',
    max_cameras         number,
    redundancy          bool
);

INSERT INTO vms_server
  SELECT
    api_url,
    auth_key,
    version,
    net_addr_list,
    resource_ptr_id,
    panic_mode,
    flags,
    system_info,
    system_name
  FROM vms_server_tmp;

INSERT INTO vms_server_user_attributes
  SELECT
    s.resource_ptr_id,
    r.guid,
    r.name,
    s.max_cameras,
    s.redundancy
  FROM vms_server_tmp s
  JOIN vms_resource r on r.id = s.resource_ptr_id;

DROP TABLE vms_server_tmp;
