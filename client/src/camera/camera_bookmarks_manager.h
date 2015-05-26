
#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

class QnCameraBookmarksManager : public QObject
{
public:
    struct FilterParameters;    /// Forward declaration of structure with filter parameters
    typedef std::function<void (bool success, const QnCameraBookmarkList &bookmarks)> BookmarksCallbackType;

    QnCameraBookmarksManager(QObject *parent = nullptr);

    virtual ~QnCameraBookmarksManager();

    /// @brief Asynchronously gathers bookmarks using specified filter
    /// @param filter Filter parameters
    /// @param clearBookmarkCache Shows if loaders should discard their caches
    /// @param callback Callback for receiving bookmarks data
    void getBookmarksAsync(const FilterParameters &filter
        , bool clearBookmarksCache
        , const BookmarksCallbackType &callback);

private:
    class Impl;
    Impl * const m_impl;
};

struct QnCameraBookmarksManager::FilterParameters
{
    QString text;
    QnVirtualCameraResourceList cameras;
    qint64 startTime;
    qint64 finishTime;
};