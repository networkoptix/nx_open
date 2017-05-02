#pragma once

#include <QtCore/QString>

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {
namespace test {
namespace utils {

bool createTestFile(const QString& fileName, qint64 size);

} // namespace utils
} // namespace test
} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
