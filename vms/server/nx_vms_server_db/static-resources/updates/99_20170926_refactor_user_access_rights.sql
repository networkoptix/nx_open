ALTER TABLE "vms_access_rights" RENAME TO "vms_access_rights_tmp";

-- Resource list granted to user. resourceIds is the vector of GUID in ubjson format
create table "vms_access_rights"(
    userOrRoleId BLOB(16) NOT NULL PRIMARY KEY,
    resourceIds BLOB);
