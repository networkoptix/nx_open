#include "random_file.h"

#include <QtCore/QFile>

#include <nx/utils/random.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace utils {

void createRandomFile(const QString& fileName, qint64 size)
{
    QFile file(fileName);

    if (!file.open(QFile::WriteOnly))
    {
        NX_ASSERT(false, "Cannot create test file.");
        return;
    }

    static constexpr qint64 kBlockSize = 1024;

    for (qint64 bytesLeft = size; bytesLeft > 0; bytesLeft -= kBlockSize)
    {
        const auto& block = random::generate(std::min(bytesLeft, kBlockSize));
        if (file.write(block.constData(), block.size()) != block.size())
        {
            NX_ASSERT(false, "Cannot write into test file.");
            return;
        }
    }

    file.close();
}

} // namespace utils
} // namespace nx
