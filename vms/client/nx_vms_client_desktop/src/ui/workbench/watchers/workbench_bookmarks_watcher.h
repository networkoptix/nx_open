// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>

#include <camera/camera_bookmarks_manager_fwd.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <utils/common/connective.h>

// TODO: #ynikitenkov Move caching from QnWorkbenchHandler to this class

class QnWorkbenchBookmarksWatcher:
    public Connective<QObject>,
    public nx::vms::client::core::CommonModuleAware
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