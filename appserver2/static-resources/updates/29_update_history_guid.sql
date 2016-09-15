ALTER TABLE "vms_cameraserveritem" RENAME TO vms_cameraserveritem_tmp;

CREATE TABLE "vms_cameraserveritem" (
    "server_guid" BLOB(16) NOT NULL,
    "timestamp" integer NOT NULL,
    "physical_id" varchar(200) NOT NULL,
    "id" integer PRIMARY KEY autoincrement);

INSERT INTO vms_cameraserveritem (server_guid, timestamp, physical_id, id)
     SELECT server_guid, timestamp, physical_id, id
       FROM vms_cameraserveritem_tmp;

DROP TABLE vms_cameraserveritem_tmp;
