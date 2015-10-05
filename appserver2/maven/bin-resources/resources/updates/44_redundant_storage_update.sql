ALTER TABLE "vms_storage" ADD COLUMN "backup" bool NOT NULL DEFAULT FALSE;

ALTER TABLE "vms_server_user_attributes" ADD COLUMN "backup_type" 				integer NOT NULL DEFAULT  0;
ALTER TABLE "vms_server_user_attributes" ADD COLUMN "backup_days_of_the_week" 	integer NOT NULL DEFAULT -1;
ALTER TABLE "vms_server_user_attributes" ADD COLUMN "backup_start" 				integer NOT NULL DEFAULT -1;
ALTER TABLE "vms_server_user_attributes" ADD COLUMN "backup_duration" 			integer NOT NULL DEFAULT -1;
ALTER TABLE "vms_server_user_attributes" ADD COLUMN "backup_bitrate" 			integer NOT NULL DEFAULT -1;