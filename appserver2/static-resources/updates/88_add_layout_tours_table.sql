-- Layout Tours Table

CREATE TABLE "vms_layout_tour_items" (
    tourId      BLOB(16) NOT NULL,                      -- tour id
    layoutId    BLOB(16) NOT NULL,                      -- layout guid
    delayMs     INTEGER NOT NULL DEFAULT 5000           -- item delay in milliseconds
    );
CREATE INDEX idx_layout_tour_items ON vms_layout_tour_items(tourId);

CREATE TABLE "vms_layout_tours" (
    id          BLOB(16) NOT NULL UNIQUE PRIMARY KEY,   -- unique tour id
    name        TEXT NULL                               -- name of the tour
    );
CREATE UNIQUE INDEX idx_vms_layout_tours ON vms_layout_tours(id);
