CREATE TABLE "storage_data" (
    camera_guid BLOB NOT NULL,                          -- GUID of the related camera resource
    role        INTEGER  NOT NULL,
    start_time  INTEGER  NOT NULL,
    timezone    INTEGER  NOT NULL,
    file_index  INTEGER  NOT NULL, 
    duration    INTEGER,
    filesize    INTEGER);
    
CREATE UNIQUE INDEX idx_storage_data ON storage_data(unique_id, role, start_time);
