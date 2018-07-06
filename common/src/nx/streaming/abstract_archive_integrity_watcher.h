#pragma once

#include <QtCore>

struct QnAviArchiveMetadata;

/**
 * @brief The AbstractArchiveIntegrityWatcher class
 * Report detected integrity problems. Report is fired only once unless reset() is called
 */
class AbstractArchiveIntegrityWatcher
{
public:
    /**
     * @brief fileRequested - call to report that file has been queyried for reading
     * @param metadata
     * @param fileName
     * @return - whether file integrity is confirmed. If not - further reading this file should be
     * rejected
     */
    virtual bool fileRequested(const QnAviArchiveMetadata& metadata, const QString& fileName) = 0;
    /**
     * @brief fileMissing - call this to report that file is missing in the archive
     */
    virtual void fileMissing(const QString& fileName) = 0;
    /**
     * @brief reset - resets watcher to the initial state so the problems detected are reported
     */
    virtual void reset() = 0;
};
