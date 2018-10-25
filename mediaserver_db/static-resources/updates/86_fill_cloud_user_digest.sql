-- Fill null/empty vms_userprofile::digest with a fixed value.

UPDATE "vms_userprofile"
SET "digest" = "password_is_in_cloud"
WHERE "is_cloud" = 1 AND coalesce("digest", '') = '';
