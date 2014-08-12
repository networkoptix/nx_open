-- Bookmark tags table
CREATE TABLE "storage_bookmark_tag" (
    id              INTEGER NOT NULL autoincrement,         -- ID to make use of full-text-search tables
    bookmark_guid   BLOB NOT NULL,
    name            TEXT NOT NULL,
    PRIMARY KEY(bookmark_guid, name)
    );

-- Bookmarks table
CREATE TABLE "storage_bookmark" (
    id              INTEGER NOT NULL autoincrement,         -- ID to make use of full-text-search tables
    guid            BLOB NOT NULL UNIQUE PRIMARY KEY,
    camera_guid     BLOB NOT NULL,                          -- GUID of the related camera resource
    start_time      INTEGER NOT NULL,
    duration        INTEGER NOT NULL,
    name            TEXT NULL,
    description     TEXT NULL,
    timeout         INTEGER NULL                            -- period of time during which the bookmarked archive part should not be deleted
    );
    
CREATE UNIQUE INDEX idx_bookmark_guid ON storage_bookmark(guid);

-- Index for faster duration-based requests
CREATE INDEX idx_bookmark_duration ON storage_bookmark(duration);
