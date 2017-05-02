#include "utils.h"

#include <QtCore/QFile>

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {
namespace test {
namespace utils {

bool createTestFile(const QString& fileName, qint64 size)
{
    QFile file(fileName);

    if (!file.open(QFile::WriteOnly))
        return false;

    for (qint64 i = 0; i < size; ++i)
    {
        const char c = static_cast<char>(rand());
        if (file.write(&c, 1) != 1)
            return false;
    }

    file.close();

    return true;
}

} // namespace utils
} // namespace test
} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
