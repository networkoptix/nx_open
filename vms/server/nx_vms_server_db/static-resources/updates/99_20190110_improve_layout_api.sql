-- Rename vms_layout::cell_spacing_width to cell_spacing, remove cell_spacing_height

ALTER TABLE vms_layout RENAME TO vms_layout_tmp;

CREATE TABLE "vms_layout" (
    "resource_ptr_id"           INTEGER PRIMARY KEY AUTOINCREMENT,
    "cell_aspect_ratio"         REAL NOT NULL DEFAULT -1.0,
    "cell_spacing"              REAL NOT NULL DEFAULT -1.0,
    "locked"                    BOOL NOT NULL DEFAULT 0,
    "background_width"          INTEGER NOT NULL DEFAULT 1,
    "background_image_filename" TEXT,
    "background_height"         INTEGER NOT NULL DEFAULT 1,
    "background_opacity"        REAL NOT NULL DEFAULT 0,
    "logical_id"                INTEGER,
    "fixed_width"               INTEGER,
    "fixed_height"              INTEGER
);


INSERT INTO vms_layout (
    resource_ptr_id,
    cell_aspect_ratio,
    cell_spacing,
    locked,
    background_width,
    background_image_filename,
    background_height,
    background_opacity
    )
    SELECT
    resource_ptr_id,
    cell_aspect_ratio,
    cell_spacing_width,
    locked,
    background_width,
    background_image_filename,
    background_height,
    background_opacity
    FROM vms_layout_tmp;

DROP TABLE vms_layout_tmp;
