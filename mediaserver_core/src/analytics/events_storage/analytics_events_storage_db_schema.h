#pragma once

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

/**
 * #CLOUD-157. Gathering cloud connect session statistics.
 */
static const char kCreateAnalyticsEventsSchema[] =
R"sql(

CREATE TABLE event(
    id                   %bigint_primary_key_auto_increment%,
    timestamp_usec_utc   BIGINT,
    duration_usec        BIGINT,
    device_guid          BLOB(16) NOT NULL,
    bounding_rectangle   VARCHAR(64),
    object_type_id       VARCHAR(1024),
    object_id            VARCHAR(1024)
);

CREATE INDEX idx_event_timestamp ON event(timestamp_usec_utc);

CREATE TABLE event_property(
    id                   %bigint_primary_key_auto_increment%,
    event_id             BIGINT,
    name                 VARCHAR(1024),
    value                VARCHAR(1024),
    FOREIGN KEY(event_id) REFERENCES event(id) ON DELETE CASCADE
);

CREATE INDEX idx_event_property_name ON event_property(name);
CREATE INDEX idx_event_property_value ON event_property(value);

)sql";

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
