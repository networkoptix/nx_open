
#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <camera/camera_bookmarks_manager_fwd.h>

#include <utils/common/connective.h>

// TODO: #ynikitenkov Move caching from QnWorkbenchHandler to this class

class QnWorkbenchBookmarksWatcher : public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT

    Q_PROPERTY(qint64 firstBookmarkUtcTimeMs READ firstBookmarkUtcTimeMs NOTIFY firstBookmarkUtcTimeMsChanged)

    typedef Connective<QObject> base_type;

public:
    static const qint64 kUndefinedTime;

public:
    QnWorkbenchBookmarksWatcher(QObject *parent = nullptr);

    virtual ~QnWorkbenchBookmarksWatcher();

    //

signals:
    void firstBookmarkUtcTimeMsChanged();

public slots:
    qint64 firstBookmarkUtcTimeMs() const;

private:
    void tryUpdateFirstBookmarkTime(qint64 utcTimeMs);

private:
    const QnCameraBookmarksQueryPtr m_minBookmarkTimeQuery;

    qint64 m_firstBookmarkUtcTimeMs;
};