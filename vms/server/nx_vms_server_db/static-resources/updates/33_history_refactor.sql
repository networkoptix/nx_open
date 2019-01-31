DROP TABLE vms_cameraserveritem;

CREATE TABLE "vms_used_cameras" (
    "server_guid" BLOB(16) PRIMARY KEY,
    "cameras" BLOB NULL);
