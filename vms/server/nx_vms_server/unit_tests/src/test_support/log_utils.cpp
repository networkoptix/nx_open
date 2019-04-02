#pragma once

namespace nx::vms::server::test::test_support {

    /** Makes logging system duplicate filtered messages to the user-provided log::Buffer */
    void createTestLogger(const QSet<QString>& filters, nx::utils::log::Buffer* outBuffer);

} // namespace nx::vms::server::test::test_support