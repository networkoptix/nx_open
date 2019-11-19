#include "file_cache.h"

#include <nx/network/socket.h>
#include <nx/utils/random.h>
#include <nx/streaming/media_data_packet.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <utils/media/ffmpeg_helper.h>
#include <utils/media/nalUnits.h>

#include "logger.h"

namespace nx::vms::testcamera {

FileCache::FileCache(QnCommonModule* commonModule):
    m_commonModule(commonModule),
    m_logger(new Logger("FileCache"))
{
}

FileCache::~FileCache()
{
}

bool FileCache::loadFile(const QString& filename)
{
    QnMutexLocker lock(&m_mutex);

    if (m_filesByFilename.find(filename) != m_filesByFilename.end()) //< Already loaded.
        return true;

    QList<QnConstCompressedVideoDataPtr> frames;
    const int fileIndex = m_filesByFilename.size();

    const QnAviResourcePtr file(new QnAviResource(filename, m_commonModule));
    QnAviArchiveDelegate aviArchiveDelegate;
    if (!aviArchiveDelegate.open(file, nullptr))
    {
        NX_LOGGER_ERROR(m_logger, "Cannot open file %1", filename);
        return false;
    }

    int64_t totalSize = 0;

    NX_LOGGER_INFO(m_logger, "Loading file #%1 %2", fileIndex, filename);

    while (const QnAbstractMediaDataPtr mediaFrame = aviArchiveDelegate.getNextData())
    {
        const auto videoFrame = std::dynamic_pointer_cast<QnCompressedVideoData>(mediaFrame);
        if (!videoFrame) //< Unneeded frame type.
            continue;

        frames.append(videoFrame);

        totalSize += videoFrame->dataSize();
        if (totalSize > 1024 * 1024 * 100)
        {
            NX_LOGGER_WARNING(m_logger, "File too large, using first ~100 MB: #%1 %2",
                fileIndex, filename);
            break;
        }
    }

    if (frames.empty())
    {
        NX_LOGGER_ERROR(m_logger, "No video frames in file #%1 %2", fileIndex, filename);
        return false;
    }

    m_filesByFilename.insert(filename, File{filename, m_filesByFilename.size(), frames});

    NX_LOGGER_INFO(m_logger, "File loaded for streaming: #%1 %2", fileIndex, filename);
    return true;
}

const FileCache::File& FileCache::getFile(const QString& filename) const
{
    QnMutexLocker lock(&m_mutex);

    const auto it = m_filesByFilename.find(filename);

    if (!NX_ASSERT(it != m_filesByFilename.end(),
        lm("File was not loaded for streaming: %1").args(filename)))
    {
        return m_undefinedFile;
    }

    return *it;
}

} // namespace nx::vms::testcamera
