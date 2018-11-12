DROP TABLE last_backup_time;
CREATE TABLE last_backup_time
(
	pool INTEGER,
    camera_id BLOB(16),
    catalog INTEGER,
    timestamp INTEGER,
    primary key(pool, camera_id, catalog)
);
