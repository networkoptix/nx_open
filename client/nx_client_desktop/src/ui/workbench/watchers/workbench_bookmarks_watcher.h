#pragma once

#include <chrono>

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>
#include <camera/camera_bookmarks_manager_fwd.h>
#include <utils/common/connective.h>

// TODO: #ynikitenkov Move caching from QnWorkbenchHandler to this class

class QnWorkbenchBookmarksWatcher : public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
    using milliseconds = std::chrono::milliseconds;

    Q_PROPERTY(milliseconds firstBookmarkUtcTime
        READ firstBookmarkUtcTime NOTIFY firstBookmarkUtcTimeChanged)

public:
    static constexpr milliseconds kUndefinedTime = milliseconds(std::numeric_limits<qint64>::max());

    QnWorkbenchBookmarksWatcher(QObject *parent = nullptr);
    virtual ~QnWorkbenchBookmarksWatcher();

    milliseconds firstBookmarkUtcTime() const;

signals:
    void firstBookmarkUtcTimeChanged();

private:
    const QnCameraBookmarksQueryPtr m_minBookmarkTimeQuery;
    milliseconds m_firstBookmarkUtcTime;

    void tryUpdateFirstBookmarkTime(milliseconds utcTime);
};