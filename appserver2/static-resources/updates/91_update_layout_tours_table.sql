-- VMS-6251 - Replacing layoutId with resourceId

DROP INDEX "idx_layout_tour_items";

ALTER TABLE "vms_layout_tour_items" RENAME TO "vms_layout_tour_items_tmp";

CREATE TABLE "vms_layout_tour_items" (
    tourId      BLOB(16) NOT NULL,                      -- tour id
    resourceId  BLOB(16) NOT NULL,                      -- resource id
    delayMs     INTEGER NOT NULL DEFAULT 5000           -- item delay in milliseconds
    );

CREATE INDEX idx_layout_tour_items ON vms_layout_tour_items(tourId);

INSERT INTO vms_layout_tour_items
  SELECT
    tourId,
    layoutId,
    delayMs
  FROM vms_layout_tour_items_tmp;

DROP TABLE vms_layout_tour_items_tmp;
