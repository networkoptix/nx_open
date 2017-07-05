-- VMS-6416 - Allow to save local files outside of media folders

-- add path column
ALTER TABLE "vms_layoutitem" ADD COLUMN "path" TEXT;