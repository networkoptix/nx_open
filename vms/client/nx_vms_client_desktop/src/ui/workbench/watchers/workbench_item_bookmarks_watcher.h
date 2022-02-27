// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnMediaResourceWidget;

// @brief Loads bookmarks for all items on current layout in vicinity of current position.
class QnWorkbenchItemBookmarksWatcher: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    explicit QnWorkbenchItemBookmarksWatcher(QObject *parent);

    virtual ~QnWorkbenchItemBookmarksWatcher();

    void setDisplayFilter(const QnVirtualCameraResourceSet& cameraFilter,
        const QString& textFilter);

private:
    class WidgetData;
    typedef QSharedPointer<WidgetData> WidgetDataPtr;
    typedef QHash<QnMediaResourceWidget *, WidgetDataPtr> WidgetDataHash;

private:
    WidgetDataHash m_widgetDataHash;
    QnVirtualCameraResourceSet m_cameraFilter; //< If empty, any is allowed.
    QString m_textFilter;
};

