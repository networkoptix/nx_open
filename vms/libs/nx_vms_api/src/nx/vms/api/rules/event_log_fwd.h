// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

namespace nx::vms::api::rules {

struct EventLogFilter;
struct EventLogRecord;
struct EventLogRecordWithLegacyTypes;

using EventLogRecordList = std::vector<EventLogRecord>;
using EventLogRecordWithLegacyTypesList = std::vector<EventLogRecordWithLegacyTypes>;

} // namespace nx::vms::rules
