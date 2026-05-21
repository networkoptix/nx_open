// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QObject>
#include <QtCore/QString>

class QImage;
class QSaveFile;

namespace nx::vms::client::desktop {

/**
 * Abstract interface for a keyed file cache: a directory of bytes-by-name. Provides a uniform
 * lookup surface so callers can resolve paths through a `FileCache*` pointer without knowing
 * the underlying storage. Local-write helpers (`storeImage`, `writeImageFile`) accept image
 * filenames only.
 */
class FileCache: public QObject
{
    Q_OBJECT

public:
    enum class OperationResult
    {
        ok,
        disconnected,
        serverError,
        fileSystemError,
        invalidOperation,
    };
    Q_ENUM(OperationResult)

    explicit FileCache(QObject* parent = nullptr);
    virtual ~FileCache() override;

    /**
     * Absolute path to `unsafeFilename` inside the cache. Returns an empty string when
     * `unsafeFilename` is not a safe leaf filename (empty, contains a path separator, equals
     * "." or ".."). Callers must handle the empty result explicitly.
     */
    virtual QString absoluteFilePath(const QString& unsafeFilename) const = 0;

    /** Reset cache bookkeeping (does not delete files on disk). */
    virtual void clear() = 0;

    /** Create the on-disk cache directory if it doesn't exist yet. Returns true on success. */
    bool ensureCacheFolder() const;

    /**
     * Atomically write `image` into the cache under `unsafeFilename` after its validation.
     */
    OperationResult storeImage(const QString& unsafeFilename, const QImage& image);

protected:
    /** Absolute path to the directory where this cache stores its files. */
    virtual QString cacheFolder() const = 0;

    /**
     * Validate `unsafeFilename` (image extension, safe leaf), open a `QSaveFile`, invoke
     * `writer` to produce the payload, and commit. On failure the on-disk file is left
     * untouched.
     */
    OperationResult writeImageFile(
        const QString& unsafeFilename,
        const std::function<bool(QSaveFile&)>& writer);
};

} // namespace nx::vms::client::desktop
