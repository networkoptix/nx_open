-- Access Rights table
CREATE TABLE "vms_access_rights" (
    user_ptr_id integer NOT NULL,               -- internal id of the user resource
    resource_ptr_id integer NOT NULL,           -- internal id of the resource, user has access to
    PRIMARY KEY(user_ptr_id, resource_ptr_id)
    );
    