-- Bookmark tags table
CREATE TABLE "storage_bookmark_tag" (
    bookmark_guid   BLOB NOT NULL,
    name            TEXT NOT NULL,
    PRIMARY KEY(bookmark_guid, name)
    );

-- Bookmarks table
CREATE TABLE "storage_bookmark" (
    guid            BLOB NOT NULL UNIQUE PRIMARY KEY,
    unique_id       TEXT NOT NULL,                          -- unique id of the related camera resource
    start_time      INTEGER NOT NULL,
    duration        INTEGER NOT NULL,
    name            TEXT NULL,
    description     TEXT NULL,
    timeout         INTEGER NULL                            -- period of time during which the bookmarked archive part should not be deleted
    );
    
CREATE UNIQUE INDEX idx_bookmark_guid ON storage_bookmark(guid);

-- Index for faster duration-based requests
CREATE INDEX idx_bookmark_duration ON storage_bookmark(duration);
