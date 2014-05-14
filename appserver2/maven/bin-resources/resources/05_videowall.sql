-- Videowall resource table
CREATE TABLE "vms_videowall" (
    autorun         bool NOT NULL DEFAULT 0,
    resource_ptr_id integer NOT NULL UNIQUE PRIMARY KEY);
    
-- OMG, should I generate this id by hands? And what if manufacturer count changes?    
INSERT INTO "vms_resourcetype" (id, name) VALUES ('633', 'Videowall');

-- Videowall-to-pc many-to-many relationship
CREATE TABLE "vms_videowall_pcs" (
    videowall_guid  BLOB(16) NOT NULL,
    pc_guid         BLOB(16) NOT NULL,
    PRIMARY KEY(videowall_guid, pc_guid)
    );
    
-- PC Screens table
CREATE TABLE "vms_videowall_screen" (
    pc_guid         BLOB(16) NOT NULL,
    pc_index        integer NOT NULL,  -- index of the screen on the given PC
    desktop_x       integer NOT NULL,
    desktop_y       integer NOT NULL,
    desktop_w       integer NOT NULL,
    desktop_h       integer NOT NULL,
    layout_x        integer NOT NULL DEFAULT 0,
    layout_y        integer NOT NULL DEFAULT 0,
    layout_w        integer NOT NULL DEFAULT -1,
    layout_h        integer NOT NULL DEFAULT -1,    
    PRIMARY KEY(pc_guid, pc_index)
    );
    
-- Videowall items table
CREATE TABLE "vms_videowall_item" (
    guid            BLOB(16) NOT NULL UNIQUE PRIMARY KEY,
    pc_guid         BLOB(16) NOT NULL,
    layout_guid     BLOB(16) NOT NULL,
    videowall_guid  BLOB(16) NOT NULL,
    name            VARCHAR(200) NOT NULL,
    x               integer NOT NULL,
    y               integer NOT NULL,
    w               integer NOT NULL,
    h               integer NOT NULL
    );

CREATE UNIQUE INDEX idx_videowall_item_guid ON vms_videowall_item(guid);

-- Videowall matrix - set of layouts attached to set of items
CREATE TABLE "vms_videowall_matrix" (
    guid            BLOB(16) NOT NULL,
    item_guid       BLOB(16) NOT NULL,
    layout_guid     BLOB(16) NOT NULL,
    PRIMARY KEY(guid, item_guid, layout_guid)
);

CREATE INDEX idx_videowall_matrix_guid ON vms_videowall_matrix(guid);
