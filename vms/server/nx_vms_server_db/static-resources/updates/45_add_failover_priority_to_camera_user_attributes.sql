ALTER TABLE "vms_camera_user_attributes"
-- Default value 2 means Qn::FP_Medium
ADD COLUMN "failover_priority" integer NOT NULL DEFAULT 2;
