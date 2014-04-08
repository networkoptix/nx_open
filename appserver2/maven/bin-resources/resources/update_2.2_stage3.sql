CREATE TABLE "vms_videowall" (
    "autorun" bool NOT NULL DEFAULT 0,
    "resource_ptr_id" integer PRIMARY KEY autoincrement);
    
INSERT INTO "vms_resourcetype" ( id,name,description,"manufacture_id" ) VALUES ( '633','Videowall',NULL,NULL );