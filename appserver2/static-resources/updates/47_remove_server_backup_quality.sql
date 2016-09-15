-- Drop backup_quality column from server attributes table
ALTER TABLE vms_server_user_attributes RENAME TO vms_server_user_attributes_tmp;

CREATE TABLE "vms_server_user_attributes" ( 
    id                      INTEGER         PRIMARY KEY AUTOINCREMENT,
    server_guid             BLOB( 16 )      NOT NULL
                                            UNIQUE,
    server_name             VARCHAR( 200 )  NOT NULL
                                            DEFAULT '''',
    max_cameras             NUMBER,
    redundancy              BOOL,
    backup_type             INTEGER         NOT NULL
                                            DEFAULT '0',
    backup_days_of_the_week INTEGER         NOT NULL
                                            DEFAULT '127',
    backup_start            INTEGER         NOT NULL
                                            DEFAULT '0',
    backup_duration         INTEGER         NOT NULL
                                            DEFAULT '-1',
    backup_bitrate          INTEGER         NOT NULL
                                            DEFAULT '-12500000'
);

-- Other properties are not published to production yet
INSERT INTO vms_server_user_attributes (server_guid, server_name, max_cameras, redundancy)
     SELECT server_guid, server_name, max_cameras, redundancy
       FROM vms_server_user_attributes_tmp;


DROP TABLE vms_server_user_attributes_tmp;
