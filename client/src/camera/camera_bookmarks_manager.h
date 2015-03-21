
#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

class QnCameraBookmarksManager : public QObject
{
public:
    struct FilterParameters;
    typedef std::function<void (bool success, const QnCameraBookmarkList &bookmarks)> BookmarksCallbackType;

    QnCameraBookmarksManager(QObject *parent);

    virtual ~QnCameraBookmarksManager();

    void getBookmarksAsync(const FilterParameters &filter
        , const BookmarksCallbackType &callback);

private:
    class Impl;
    Impl * const m_impl;
};

struct QnCameraBookmarksManager::FilterParameters
{
    QString text;
    QnResourceList cameras;
    qint64 startTime;
    qint64 finishTime;
};