#include "utils.h"

#include <QtCore/QFile>

#include <nx/utils/log/assert.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {
namespace test {
namespace utils {

void createTestFile(const QString& fileName, qint64 size)
{
    QFile file(fileName);

    if (!file.open(QFile::WriteOnly))
    {
        NX_ASSERT(false, "Cannot create test file.");
        return;
    }

    for (qint64 i = 0; i < size; ++i)
    {
        const char c = static_cast<char>(rand());
        if (file.write(&c, 1) != 1)
        {
            NX_ASSERT(false, "Cannot write into test file.");
            return;
        }
    }

    file.close();
}

} // namespace utils
} // namespace test
} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
