CREATE TABLE last_backup_time
(
    camera_id BLOB(16),
    catalog INTEGER,
    timestamp INTEGER,
    primary key(camera_id, catalog, timestamp)
);
