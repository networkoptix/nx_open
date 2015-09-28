ALTER TABLE "vms_storage" ADD COLUMN "redundant" bool NOT NULL DEFAULT FALSE;
ALTER TABLE "vms_storage" ADD COLUMN "redundant_days_of_the_week" integer NOT NULL DEFAULT -1;
ALTER TABLE "vms_storage" ADD COLUMN "redundant_start" integer NOT NULL DEFAULT -1;
ALTER TABLE "vms_storage" ADD COLUMN "redundant_duration" integer NOT NULL DEFAULT -1;