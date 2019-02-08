-- Change column "unique_id" (string) to "camera_guid" (guid) - part 2 (after C++ code).

DROP TABLE "bookmarks_tmp";

CREATE UNIQUE INDEX idx_bookmark_guid ON bookmarks(guid);
CREATE INDEX idx_bookmark_duration ON bookmarks(duration);
CREATE INDEX idx_bookmark_cam ON bookmarks(camera_guid, start_time);
