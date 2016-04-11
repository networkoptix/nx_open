-- Access Rights table
CREATE TABLE "vms_user_groups" (
    id          BLOB(16) NOT NULL UNIQUE PRIMARY KEY,   -- unique group id
    name        TEXT NULL,                              -- name of the group
    permissions INTEGER NOT NULL DEFAULT 0              -- permissions flags set
    );
    
CREATE UNIQUE INDEX idx_user_groups_guid ON vms_user_groups(id);

ALTER TABLE "vms_userprofile" ADD COLUMN "group" BLOB(16) NULL;
