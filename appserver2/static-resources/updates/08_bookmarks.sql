-- Camera bookmark tags usage table
CREATE TABLE "vms_camera_bookmark_tag" (
    name            TEXT NOT NULL UNIQUE PRIMARY KEY
    );

-- Bookmark tags table
CREATE TABLE "vms_bookmark_tag" (
    bookmark_guid   BLOB NOT NULL,
    name            TEXT NOT NULL,
    PRIMARY KEY(bookmark_guid, name)
    );

-- Bookmarks table
CREATE TABLE "vms_bookmark" (
    guid            BLOB NOT NULL UNIQUE PRIMARY KEY,
    camera_id       INTEGER NOT NULL,                       -- inner ID of the related camera resource
    start_time      INTEGER NOT NULL,
    duration        INTEGER NOT NULL,
    name            TEXT NULL,
    description     TEXT NULL,
    timeout         INTEGER NULL                            -- period of time during which the bookmarked archive part should not be deleted
    );
    
CREATE UNIQUE INDEX idx_bookmark_guid ON vms_bookmark(guid);

-- Index for faster TOP-100 requests
CREATE INDEX idx_bookmark_duration ON vms_bookmark(duration);
