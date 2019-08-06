#pragma once

#include <nx/utils/log/log_level.h>

namespace nx::vms::server::test::test_support {

/** Makes logging system duplicate filtered messages to the user-provided log::Buffer. */
void createTestLogger(
    const std::set<nx::utils::log::Filter>& logFilters, nx::utils::log::Buffer** outBuffer);

} // namespace nx::vms::server::test::test_support
