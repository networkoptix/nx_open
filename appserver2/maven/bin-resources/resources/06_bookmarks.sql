-- Camera bookmark tags usage table
CREATE TABLE "vms_camera_bookmark_tag" (
    name            VARCHAR(30) NOT NULL UNIQUE PRIMARY KEY
    );

-- Bookmark tags table
CREATE TABLE "vms_bookmark_tag" (
    bookmark_guid   BLOB(16) NOT NULL,
    name            VARCHAR(30) NOT NULL,
    PRIMARY KEY(bookmark_guid, name)
    );

-- Bookmarks table
CREATE TABLE "vms_bookmark" (
    guid            BLOB(16) NOT NULL UNIQUE PRIMARY KEY,
    camera_id       integer NOT NULL,                       -- inner ID of the related camera resource
    start_time      integer NOT NULL,
    duration        integer NOT NULL,
    name            VARCHAR(30) NULL,
    description     VARCHAR(200) NULL,
    timeout         integer NULL                            -- period of time during which the bookmarked archive part should not be deleted
    );
    
CREATE UNIQUE INDEX idx_bookmark_guid ON vms_bookmark(guid);

-- Index for faster TOP-100 requests
CREATE INDEX idx_bookmark_duration ON vms_bookmark(duration);
