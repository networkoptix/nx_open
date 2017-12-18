#pragma once

namespace nx {
namespace analytics {
namespace storage {

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

// VMS-7876
static const char kAnalyticsDbMoreIndexes[] =
R"sql(

CREATE INDEX idx_event_object_id ON event(object_id);
CREATE INDEX idx_event_device_guid ON event(device_guid);

)sql";

} // namespace storage
} // namespace analytics
} // namespace nx
