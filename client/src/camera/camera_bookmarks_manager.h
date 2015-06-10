#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <recording/time_period.h>

class QnCameraBookmarksManager : public QObject
{
    Q_OBJECT
public:


    struct FilterParameters;    /// Forward declaration of structure with filter parameters
    typedef std::function<void (bool success, const QnCameraBookmarkList &bookmarks)> BookmarksCallbackType;

    QnCameraBookmarksManager(QObject *parent = nullptr);

    virtual ~QnCameraBookmarksManager();

    /// @brief Asynchronously gathers bookmarks using specified filter
    /// @param filter Filter parameters
    /// @param callback Callback for receiving bookmarks data
    void getBookmarksAsync(const QnVirtualCameraResourceList &cameras, const FilterParameters &filter, int limit, BookmarksCallbackType callback);

private:
    class Impl;
    Impl * const m_impl;
};

struct QnCameraBookmarksManager::FilterParameters {
    QString text;
    QnTimePeriod period;
    int limit;
    Qn::BookmarkSearchStrategy strategy;

    FilterParameters();
};