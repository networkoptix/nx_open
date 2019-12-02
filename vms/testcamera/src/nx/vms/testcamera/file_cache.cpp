#include "file_cache.h"

#include <nx/network/socket.h>
#include <nx/utils/random.h>
#include <nx/streaming/media_data_packet.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <utils/media/ffmpeg_helper.h>
#include <utils/media/nalUnits.h>
#include <nx/streaming/config.h> //< for CL_MAX_CHANNELS

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
    std::vector<std::shared_ptr<const QnCompressedVideoData>>* frames,
    std::set<int>* channelNumbers) const
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

    while (const auto mediaFrame = aviArchiveDelegate.getNextData())
    {
        const auto videoFrame = std::dynamic_pointer_cast<const QnCompressedVideoData>(mediaFrame);
        if (!videoFrame) //< Unneeded frame type.
            continue;

        totalSize += videoFrame->dataSize();
        if (m_maxFileSizeMegabytes > 0 && totalSize > 1024 * 1024 * m_maxFileSizeMegabytes)
        {
            NX_LOGGER_WARNING(m_logger, "File too large, using first ~%1 MB.",
                m_maxFileSizeMegabytes);
            break;
        }

        if (videoFrame->channelNumber >= CL_MAX_CHANNELS)
        {
            NX_LOGGER_WARNING(m_logger,
                "Channel number in frame #%1 is %2 which is larger than %3; ignoring the frame.",
                frameIndex, videoFrame->channelNumber, CL_MAX_CHANNELS - 1);
            continue;
        }

        if (const auto [_, isNew] = channelNumbers->insert(videoFrame->channelNumber); isNew)
            NX_LOGGER_VERBOSE(m_logger, "Detected new channel #%1.", videoFrame->channelNumber);

        frames->push_back(videoFrame);

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
    file.index = (int) m_filesByFilename.size();

    const auto fileLoggerContext = m_logger->pushContext(
        lm("File #%1 %2").args(file.index, file.filename));

    NX_LOGGER_VERBOSE(m_logger, "Loading file.");

    std::set<int> channelNumbers;

    if (!loadVideoFrames(filename, &file.frames, &channelNumbers))
        return false;

    if (!NX_ASSERT(!channelNumbers.empty()))
        return false;

    file.channelCount = /*max*/ *channelNumbers.rbegin() + 1;
    if (!NX_ASSERT(file.channelCount >= 0, "%1", file.channelCount))
        return false;

    // If there are gaps in detected channel numbers, report them as warnings.
    for (int i = 0; i < file.channelCount; ++i)
    {
        if (!channelNumbers.count(i)) //< Gap in the set.
            NX_LOGGER_WARNING(m_logger, "Channel #%1 has no video frames.");
    }

    if (file.frames.empty())
    {
        NX_LOGGER_ERROR(m_logger, "No video frames found.");
        return false;
    }

    m_filesByFilename.insert({filename, file});

    NX_LOGGER_INFO(m_logger, "File loaded for streaming%1.",
        (file.channelCount > 1) ? lm("; detected %1 channels").args(file.channelCount) : "");
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

    return it->second;
}

} // namespace nx::vms::testcamera
