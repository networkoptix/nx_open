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
    end_time        integer NULL,
    name            VARCHAR(30) NULL,
    description     VARCHAR(200) NULL,
    color           integer NOT NULL,
    lock_time       integer NULL                            -- period of time during which the bookmarked archive part should not be deleted
    );
    
CREATE UNIQUE INDEX idx_bookmark_guid ON vms_bookmark(guid);