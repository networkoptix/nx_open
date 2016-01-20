
#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

class QnCurrentLayoutBookmarksCache;

class QnCurrentLayoutBookmarksWatcher : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnCurrentLayoutBookmarksWatcher(QObject *parent);

    virtual ~QnCurrentLayoutBookmarksWatcher();

    void setPosition(qint64 pos);

signals:
    void onBookmarksChanged(const QnVirtualCameraResource &camera
        , const QnCameraBookmarkList &bookmarks);

private:
    typedef QScopedPointer<QnCurrentLayoutBookmarksCache> QnCurrentLayoutBookmarksCachePtr;

    QnCurrentLayoutBookmarksCachePtr m_bookmarksCache;
};

