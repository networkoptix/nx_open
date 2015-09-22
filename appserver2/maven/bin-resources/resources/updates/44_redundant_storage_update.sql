ALTER TABLE "vms_storage" ADD COLUMN "redundant" bool NOT NULL DEFAULT FALSE;
ALTER TABLE "vms_storage" ADD COLUMN "redundant_start_time" integer NOT NULL DEFAULT -1;
ALTER TABLE "vms_storage" ADD COLUMN "redundant_duration" integer NOT NULL DEFAULT -1;
ALTER TABLE "vms_storage" ADD COLUMN "redundant_period" integer NOT NULL DEFAULT -1;