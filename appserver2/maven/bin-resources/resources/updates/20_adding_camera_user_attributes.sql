DROP table "vms_camera";
CREATE TABLE "vms_camera" (
    resource_ptr_id     integer         PRIMARY KEY AUTOINCREMENT,
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

DROP TABLE "vms_scheduletask";
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



DROP TABLE "vms_server";
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
