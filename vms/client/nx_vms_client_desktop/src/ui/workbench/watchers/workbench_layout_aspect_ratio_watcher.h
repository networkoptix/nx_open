// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <nx/vms/client/desktop/window_context_aware.h>

class QnWorkbenchRenderWatcher;
class QnResourceWidget;
class QnWorkbenchLayout;

class QnWorkbenchLayoutAspectRatioWatcher:
    public QObject,
    public nx::vms::client::desktop::WindowContextAware
{
    Q_OBJECT

public:
    explicit QnWorkbenchLayoutAspectRatioWatcher(
        nx::vms::client::desktop::WindowContext* windowContext,
        QObject* parent = nullptr);
    virtual ~QnWorkbenchLayoutAspectRatioWatcher() override;

private slots:
    void at_renderWatcher_widgetChanged(QnResourceWidget* widget);
    void at_resourceWidget_aspectRatioChanged();
    void at_resourceWidget_destroyed();
    void at_workbench_currentLayoutChanged();
    void at_workbench_currentLayoutAboutToBeChanged();
    void at_watchedLayout_cellAspectRatioChanged();

private:
    void watchCurrentLayout();
    void unwatchCurrentLayout();
    void setAppropriateAspectRatio(QnResourceWidget* widget);

private:
    QnWorkbenchRenderWatcher* const m_renderWatcher;
    QnWorkbenchLayout* m_watchedLayout = nullptr;
    QSet<QnResourceWidget*> m_watchedWidgets;
    bool m_monitoring = true;
};
