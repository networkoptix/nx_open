#pragma once

#include <set>
#include <vector>
#include <map>
#include <memory>

#include <QFile>

#include <nx/streaming/video_data_packet.h>
#include <nx/utils/thread/mutex.h>

class QnCommonModule;

namespace nx::vms::testcamera {

class Logger; //< private

/**
 * Loads video files into memory, limiting them in size to the specified value.
 *
 * Thread-safe.
 */
class FileCache
{
public:
    struct File
    {
        QString filename = "UNINITIALIZED";
        int index = -1; //< Used for logging.
        int channelCount = -1; //< Number of independent video channels in the file.
        std::vector<std::shared_ptr<const QnCompressedVideoData>> frames;
    };

    FileCache(QnCommonModule* commonModule, int maxFileSizeMegabytes);
    ~FileCache();

    /** @return False on error, having logged the error message. */
    bool loadFile(const QString& filename);

    /** If the file was not previously loaded, an assertion fails. */
    const File& getFile(const QString& filename) const;

    int fileCount() const { return (int) m_filesByFilename.size(); }

private:
    bool loadVideoFrames(
        const QString& filename,
        std::vector<std::shared_ptr<const QnCompressedVideoData>>* frames,
        std::set<int>* channelNumbers) const;

private:
    QnCommonModule* const m_commonModule;
    const int m_maxFileSizeMegabytes;
    const std::unique_ptr<Logger> m_logger;

    std::map<QString, File> m_filesByFilename;
    mutable QnMutex m_mutex;

    const File m_undefinedFile; //< Returned by ref on assertion failures.
};

} // namespace nx::vms::testcamera
