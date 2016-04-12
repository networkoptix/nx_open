-- Access Rights table
DROP TABLE "vms_access_rights";

CREATE TABLE "vms_access_rights" (
    guid            BLOB(16) NOT NULL,      -- unique id of the user or the user group
    resource_ptr_id integer NOT NULL,       -- internal id of the resource, user or group has access to
    PRIMARY KEY(guid, resource_ptr_id)
    );
    
CREATE UNIQUE INDEX idx_access_rights_guid ON vms_access_rights(guid);