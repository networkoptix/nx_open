ALTER TABLE vms_userprofile ADD COLUMN is_ldap BOOLEAN NOT NULL DEFAULT 0;
ALTER TABLE vms_userprofile ADD COLUMN is_enabled BOOLEAN NOT NULL DEFAULT 1;