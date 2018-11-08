ALTER TABLE bookmarks ADD end_time INTEGER;
CREATE INDEX idx_bookmark_end_time ON bookmarks(camera_guid, end_time);
UPDATE bookmarks set end_time = start_time + duration;
