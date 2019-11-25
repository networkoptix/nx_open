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

FileCache::FileCache(QnCommonModule* commonModule, int maxFileSizeMegabytes):
    m_commonModule(commonModule),
    m_maxFileSizeMegabytes(maxFileSizeMegabytes),
    m_logger(new Logger("FileCache"))
{
}

FileCache::~FileCache()
{
}

bool FileCache::loadVideoFrames(
    const QString& filename,
    QList<QnConstCompressedVideoDataPtr>* frames)
{
    const QnAviResourcePtr aviResource(new QnAviResource(filename, m_commonModule));
    QnAviArchiveDelegate aviArchiveDelegate;
    if (!aviArchiveDelegate.open(aviResource))
    {
        NX_LOGGER_ERROR(m_logger, "Cannot open file.");
        return false;
    }

    int64_t totalSize = 0;
    int frameIndex = 0;

    while (const QnAbstractMediaDataPtr mediaFrame = aviArchiveDelegate.getNextData())
    {
        const auto videoFrame = std::dynamic_pointer_cast<QnCompressedVideoData>(mediaFrame);
        if (!videoFrame) //< Unneeded frame type.
            continue;

        totalSize += videoFrame->dataSize();
        if (m_maxFileSizeMegabytes > 0 && totalSize > 1024 * 1024 * m_maxFileSizeMegabytes)
        {
            NX_LOGGER_WARNING(m_logger, "File too large, using first ~%1 MB.",
                m_maxFileSizeMegabytes);
            break;
        }

        frames->append(videoFrame);

        ++frameIndex;
    }

    return true;
}

bool FileCache::loadFile(const QString& filename)
{
    QnMutexLocker lock(&m_mutex);

    if (m_filesByFilename.find(filename) != m_filesByFilename.end()) //< Already loaded.
        return true;

    File file;
    file.filename = filename;
    file.index = m_filesByFilename.size();

    const auto fileLoggerContext = m_logger->pushContext(
        lm("File #%1 %2").args(file.index, file.filename));

    NX_LOGGER_VERBOSE(m_logger, "Loading file.");

    if (!loadVideoFrames(filename, &file.frames))
        return false;

    if (file.frames.empty())
    {
        NX_LOGGER_ERROR(m_logger, "No video frames found.");
        return false;
    }

    m_filesByFilename.insert(filename, file);

    NX_LOGGER_INFO(m_logger, "File loaded for streaming.");
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
