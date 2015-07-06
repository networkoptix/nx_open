-- Clients Information table
CREATE TABLE "vms_client_infos" (
    guid            BLOB NOT NULL UNIQUE PRIMARY KEY,
    parent_guid     BLOB NOT NULL,
    skin            TEXT NULL,
    systemInfo      TEXT NULL,
    cpuArchitecture TEXT NULL,
    cpuModelName    TEXT NULL,
    phisicalMemory  integer NULL,
    openGLVersion   TEXT NULL,
    openGLVendor    TEXT NULL,
    openGLRenderer  TEXT NULL
    );
