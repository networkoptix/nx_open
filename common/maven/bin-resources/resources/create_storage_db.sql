CREATE TABLE storage_data (unique_id  string NOT NULL,
			   role       int  NOT NULL,
			   start_time int  NOT NULL,
                           timezone   int  NOT NULL,
                           file_index int  NOT NULL, 
			   duration   int,
			   filesize   number);
CREATE UNIQUE INDEX idx_storage_data ON storage_data(unique_id, role, start_time);
