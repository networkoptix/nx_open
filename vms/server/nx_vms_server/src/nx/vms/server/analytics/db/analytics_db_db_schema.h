#pragma once

namespace nx::analytics::db {

static const char kCreateAnalyticsEventsSchema[] =
R"sql(

CREATE TABLE event(
    timestamp_usec_utc   BIGINT,
    duration_usec        BIGINT,
    device_guid          BLOB(16) NOT NULL,
    object_type_id       VARCHAR(1024),
    object_id            VARCHAR(1024),
    attributes           TEXT,
    box_top_left_x       INTEGER,
    box_top_left_y       INTEGER,
    box_bottom_right_x   INTEGER,
    box_bottom_right_y   INTEGER
);

CREATE INDEX idx_event_timestamp ON event(timestamp_usec_utc);

CREATE VIRTUAL TABLE event_properties USING fts4(content);

)sql";

//-------------------------------------------------------------------------------------------------
// VMS-7876
static const char kAnalyticsDbMoreIndexes[] =
R"sql(

CREATE INDEX idx_event_object_id ON event(object_id);
CREATE INDEX idx_event_device_guid ON event(device_guid);

)sql";

//-------------------------------------------------------------------------------------------------
// VMS-7876
static const char kAnalyticsDbEvenMoreIndexes[] =
R"sql(

CREATE INDEX idx_event_for_streaming_cursor ON event(device_guid, timestamp_usec_utc);

)sql";

//-------------------------------------------------------------------------------------------------
// META-220
static const char kMoveAttributesTextToASeparateTable[] =
R"sql(

CREATE TABLE unique_attributes(id INTEGER PRIMARY KEY, content TEXT);
CREATE INDEX unique_attributes_content_idx on unique_attributes(content);
INSERT INTO unique_attributes (content) SELECT DISTINCT attributes FROM event;

ALTER TABLE event add column attributes_id integer;
CREATE INDEX event_attributes_id on event (attributes_id);

UPDATE event SET attributes_id = (SELECT id from unique_attributes WHERE content = attributes);
UPDATE event SET attributes = NULL;

DROP TABLE event_properties;

CREATE VIRTUAL TABLE attributes_text_index USING fts4(content);
INSERT INTO attributes_text_index(docid, content) SELECT id, content FROM unique_attributes;

)sql";

//-------------------------------------------------------------------------------------------------
// META-220
// - moving object_type to a separate table
// - dropping event.attributes column
// - moving device_guid to a separate table
static const char kMoveObjectTypeAndDeviceToSeparateTables[] =
R"sql(

CREATE TABLE object_type (id INTEGER PRIMARY KEY, name text);
INSERT INTO object_type(name) SELECT DISTINCT object_type_id FROM event;

CREATE TABLE device (id INTEGER PRIMARY KEY, guid BLOB(16) NOT NULL);
INSERT INTO device (guid) SELECT DISTINCT device_guid FROM event;

ALTER TABLE event ADD COLUMN device_id INTEGER;
UPDATE event SET device_id = (SELECT id FROM device WHERE guid=device_guid);

CREATE TABLE event_new(
    timestamp_usec_utc   BIGINT,
    duration_usec        BIGINT,
    device_id            INTEGER,
    object_type_id       INTEGER,
    object_id            VARCHAR(1024),
    attributes_id        INTEGER,
    box_top_left_x       INTEGER,
    box_top_left_y       INTEGER,
    box_bottom_right_x   INTEGER,
    box_bottom_right_y   INTEGER
);

INSERT INTO event_new(
    timestamp_usec_utc, duration_usec, device_id, object_type_id, object_id,
    attributes_id, box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y)
SELECT timestamp_usec_utc, duration_usec, device_id, ot.id, object_id,
     attributes_id, box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y
FROM event e, object_type ot
WHERE e.object_type_id = ot.name;

DROP INDEX idx_event_timestamp;
DROP INDEX idx_event_object_id;
DROP INDEX idx_event_device_guid;
DROP INDEX idx_event_for_streaming_cursor;

DROP TABLE event;
ALTER TABLE event_new RENAME TO event;

CREATE INDEX idx_event_timestamp ON event(timestamp_usec_utc);
CREATE INDEX idx_event_object_id ON event(object_id);
CREATE INDEX idx_event_device_id ON event(device_id);
CREATE INDEX idx_event_for_streaming_cursor ON event(device_id, timestamp_usec_utc);

)sql";

//-------------------------------------------------------------------------------------------------
// META-223
// TODO: #ak Rename timestamp_usec_utc along with the next event table structure change.
static constexpr char kConvertTimestampToMillis[] =
R"sql(

UPDATE event SET timestamp_usec_utc = timestamp_usec_utc / 1000;

)sql";

//-------------------------------------------------------------------------------------------------
// META-223
static constexpr char kPackCoordinates[] =
R"sql(

UPDATE event SET
    box_top_left_x = round(box_top_left_x * %1),
    box_top_left_y = round(box_top_left_y * %1),
    box_bottom_right_x = round(box_bottom_right_x * %1),
    box_bottom_right_y = round(box_bottom_right_y * %1);

)sql";

//-------------------------------------------------------------------------------------------------
// META-226
static constexpr char kAddFullTimePeriods[] =
R"sql(

CREATE TABLE time_period_full(
    id INTEGER PRIMARY KEY,
    device_id          INTEGER,
    period_start_ms    INTEGER,
    period_end_ms      INTEGER
);

CREATE INDEX idx_time_period_full_device_id ON time_period_full(device_id);

INSERT INTO time_period_full (device_id, period_start_ms, period_end_ms)
SELECT device_id, MIN(timestamp_usec_utc), MAX(timestamp_usec_utc)
FROM event
GROUP BY device_id, timestamp_usec_utc/60000;

)sql";

//-------------------------------------------------------------------------------------------------
// META-223
// NOTE: Not renaming duration_usec since renaming a field is complicated and the next scheme update
// just drops the 'event' table.
static constexpr char kConvertDurationToMillis[] =
R"sql(

UPDATE event SET duration_usec = duration_usec / 1000;

)sql";

//-------------------------------------------------------------------------------------------------
// META-225
// TODO: Add indexes.
static constexpr char kSplitDataToObjectAndSearch[] =
R"sql(

DROP INDEX IF EXISTS idx_event_timestamp;
DROP INDEX IF EXISTS idx_event_object_id;
DROP INDEX IF EXISTS idx_event_device_id;
DROP INDEX IF EXISTS idx_event_for_streaming_cursor;
DROP INDEX IF EXISTS event_attributes_id;
DROP TABLE event;

CREATE TABLE object(
    id                          INTEGER PRIMARY KEY,
    device_id                   INTEGER,
    object_type_id              INTEGER,
    guid                        BLOB,
    track_start_ms              INTEGER,
    track_end_ms                INTEGER,
    track_detail                BLOB,
    attributes_id               INTEGER
);

CREATE INDEX idx_object_object_type_id ON object(object_type_id);
CREATE INDEX idx_object_attributes_id ON object(attributes_id);
CREATE INDEX idx_object_device_id_track_start_ms ON object(
    device_id, track_start_ms);

CREATE TABLE object_search(
    id                          INTEGER PRIMARY KEY,
    timestamp_seconds_utc       INTEGER,
    box_top_left_x              INTEGER,
    box_top_left_y              INTEGER,
    box_bottom_right_x          INTEGER,
    box_bottom_right_y          INTEGER
);

CREATE INDEX idx_object_search_box_timestamp ON object_search(
    box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y,
    timestamp_seconds_utc);

CREATE TABLE object_search_to_object(
    object_search_id            INTEGER,
    object_id                   INTEGER
);

CREATE INDEX idx_object_search_to_object_full ON object_search_to_object(
    object_search_id, object_id);

)sql";

//-------------------------------------------------------------------------------------------------
// META-246.
static constexpr char kObjectBestShot[] =
R"sql(

ALTER TABLE object ADD COLUMN best_shot_timestamp_ms INTEGER;
ALTER TABLE object ADD COLUMN best_shot_rect BLOB;

)sql";

//-------------------------------------------------------------------------------------------------
// META-254.
static constexpr char kCombinedAttributes[] =
R"sql(

CREATE TABLE combined_attributes(
    combination_id      INTEGER,
    attributes_id       INTEGER
);

CREATE INDEX idx_combined_attributes_full ON combined_attributes(
    attributes_id, combination_id);

)sql";

//-------------------------------------------------------------------------------------------------
// META-281.
static constexpr char kFixObjectSearchToObjectIndices[] =
R"sql(

DROP INDEX idx_object_search_to_object_full;

CREATE INDEX idx_object_search_to_object_object_search_id ON object_search_to_object(
    object_search_id);

CREATE INDEX idx_object_search_to_object_object_id ON object_search_to_object(
    object_id);

)sql";

} // namespace nx::analytics::db
