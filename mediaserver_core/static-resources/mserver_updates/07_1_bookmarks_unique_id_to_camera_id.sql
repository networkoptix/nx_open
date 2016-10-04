-- Change column "unique_id" (string) to "camera_id" (guid) - part 1 (before C++ code).
-- ATTENTION: Values of "camera_id" are calculated by C++ code as MD5(unique_id).

DROP INDEX idx_bookmark_guid;
DROP INDEX idx_bookmark_duration;
DROP INDEX idx_bookmark_cam;

ALTER TABLE "bookmarks" RENAME TO "bookmarks_tmp";

CREATE TABLE "bookmarks"
(
    guid BLOB NOT NULL UNIQUE PRIMARY KEY,
    camera_id BLOB(16),
    start_time INTEGER NOT NULL,
    duration INTEGER NOT NULL,
    name TEXT NULL,
    description TEXT NULL,
    timeout INTEGER NULL -- period of time during which the bookmarked archive part should not be deleted
);

INSERT INTO "bookmarks"
    SELECT
        guid,
        NULL, -- camera_id
        start_time,
        duration,
        name,
        description,
        timeout
    FROM "bookmarks_tmp";
