-- VMS-6112 - Make layout tours personal

-- add parent id column
ALTER TABLE "vms_layout_tours" ADD COLUMN "parentId" BLOB(16);
