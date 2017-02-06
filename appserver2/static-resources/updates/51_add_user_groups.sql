-- User Groups table
CREATE TABLE "vms_user_groups" (
    id          BLOB(16) NOT NULL UNIQUE PRIMARY KEY,   -- unique group id
    name        TEXT NULL,                              -- name of the group
    permissions INTEGER NOT NULL DEFAULT 0              -- permissions flags set
    );
CREATE UNIQUE INDEX idx_user_groups_guid ON vms_user_groups(id);
    
-- Access Rights table (dropping prev version) 
DROP TABLE "vms_access_rights";

CREATE TABLE "vms_access_rights" (
    guid            BLOB(16) NOT NULL,      -- unique id of the user or the user group
    resource_ptr_id integer NOT NULL,       -- internal id of the resource, user or group has access to
    PRIMARY KEY(guid, resource_ptr_id)
    );
CREATE INDEX idx_access_rights_guid ON vms_access_rights(guid);    

ALTER TABLE "vms_userprofile" ADD COLUMN "group_guid" BLOB(16) NULL;
ALTER TABLE "vms_userprofile" ADD COLUMN "is_cloud" BOOL NULL;