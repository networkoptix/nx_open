DROP INDEX idx_bookmark_duration;
CREATE INDEX idx_bookmark_duration ON bookmarks(camera_guid, duration);
