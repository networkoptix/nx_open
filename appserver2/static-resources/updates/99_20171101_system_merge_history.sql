create table "system_merge_history" (
    "id" INTEGER PRIMARY KEY AUTOINCREMENT,
    "timestamp" INTEGER NOT NULL,
    merged_system_local_id TEXT NOT NULL,
    merged_system_cloud_id TEXT,
    username TEXT NOT NULL,
    signature TEXT
);
