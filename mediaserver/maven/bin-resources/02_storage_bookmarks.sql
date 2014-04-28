-- Bookmark tags table
CREATE TABLE "storage_bookmark_tag" (
    bookmark_guid   BLOB(16) NOT NULL,
    name            VARCHAR(30) NOT NULL,
    PRIMARY KEY(bookmark_guid, name)
    );

-- Bookmarks table
CREATE TABLE "storage_bookmark" (
    guid            BLOB(16) NOT NULL UNIQUE PRIMARY KEY,
    camera_id       VARCHAR(64) NOT NULL,                   -- ID of the related camera resource TODO: #vasilenko change to guid
    start_time      INTEGER NOT NULL,
    duration        INTEGER NOT NULL,
    name            VARCHAR(30) NULL,
    description     VARCHAR(200) NULL,
    timeout         INTEGER NULL                            -- period of time during which the bookmarked archive part should not be deleted
    );
    
CREATE UNIQUE INDEX idx_bookmark_guid ON vms_bookmark(guid);

-- Index for faster TOP-100 requests
CREATE INDEX idx_bookmark_duration ON vms_bookmark(duration);
