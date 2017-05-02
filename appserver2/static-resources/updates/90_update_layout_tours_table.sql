-- VMS-6116 - Allow to disable autoswitching in tour settings

-- add settings column, will contain json text to support extensions
ALTER TABLE "vms_layout_tours" ADD COLUMN "settings" TEXT NULL;
