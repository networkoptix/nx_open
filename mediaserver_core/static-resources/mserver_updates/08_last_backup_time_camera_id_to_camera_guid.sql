ALTER TABLE "last_backup_time" RENAME TO "last_backup_time_tmp";

CREATE TABLE "last_backup_time"
(
    pool INTEGER,
    camera_guid BLOB(16),
    catalog INTEGER,
    timestamp INTEGER,
    primary key(pool, camera_guid, catalog)
);

INSERT INTO "last_backup_time"
    SELECT * FROM "last_backup_time_tmp";

DROP TABLE "last_backup_time_tmp";
