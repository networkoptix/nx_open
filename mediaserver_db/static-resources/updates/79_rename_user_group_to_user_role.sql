-- Rename vms_user_groups to vms_user_roles, and idx_user_groups_guid to idx_user_roles_guid.

DROP INDEX "idx_user_groups_guid";

CREATE TABLE "vms_user_roles" (
    id BLOB(16) NOT NULL UNIQUE PRIMARY KEY, -- unique user role id
    name TEXT NULL, -- name of the user role
    permissions INTEGER NOT NULL DEFAULT 0 -- permissions flags set
);

INSERT INTO "vms_user_roles"
    SELECT * FROM "vms_user_groups";

DROP TABLE "vms_user_groups";

CREATE UNIQUE INDEX idx_user_roles_guid ON vms_user_roles(id);

---------------------------------------------------------------------------------------------------
-- Rename vms_userprofile::group_guid to user_role_guid.

ALTER TABLE "vms_userprofile" RENAME TO "vms_userprofile_tmp";

CREATE TABLE "vms_userprofile" (
    "user_id" integer NOT NULL UNIQUE,
    "digest" varchar(128) NULL,
    "resource_ptr_id" integer PRIMARY KEY autoincrement,
    "rights" integer unsigned NOT NULL DEFAULT 0,
    crypt_sha512_hash varchar(128) NOT NULL DEFAULT '',
    is_ldap BOOLEAN NOT NULL DEFAULT 0,
    is_enabled BOOLEAN NOT NULL DEFAULT 1,
    realm varchar(128) NOT NULL DEFAULT 'NetworkOptix',
    "user_role_guid" BLOB(16) NULL,
    "is_cloud" BOOL NULL,
    "full_name" VARCHAR(200) NULL
);

INSERT INTO "vms_userprofile"
    SELECT * FROM "vms_userprofile_tmp";

DROP TABLE "vms_userprofile_tmp";

