// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QModelIndex>
#include <QtCore/QTimer>
#include <QtGui/QStandardItem>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/rules/event_log_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/rules/rules_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class EventLogDialog; }

struct QnTimePeriod;

namespace nx::vms::client::desktop {

class EventLogModel;

class EventLogDialog: public QnSessionAwareDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareDialog;

public:
    EventLogDialog(QWidget* parent);
    virtual ~EventLogDialog();

    void disableUpdateData();
    void enableUpdateData();
    void setDateRange(qint64 startTimeMs, qint64 endTimeMs);
    const QnUuidSet& eventDevices() const;
    void setEventDevices(const QnUuidSet& ids);
    void setActionType(const QString& value);
    void setEventType(const QString& value);
    void setAnalyticsEventType(nx::vms::api::analytics::EventTypeId value);
    void setText(const QString& text);

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;

private:
    void retranslateUi();
    void initEventsModel();
    void initActionsModel();

    void reset();
    void updateData();
    void updateDataDelayed();
    void at_eventsGrid_clicked(const QModelIndex& index);
    void at_eventsGrid_customContextMenuRequested(const QPoint& screenPos);
    void at_cameraButton_clicked();
    void at_filterAction_triggered();
    void at_clipboardAction_triggered();
    void at_exportAction_triggered();
    void at_mouseButtonRelease(QObject* sender, QEvent* event);

    QStandardItem* createEventTree(const nx::vms::rules::Group& group);
    void createAnalyticsEventTree(QStandardItem* rootItem);
    void updateAnalyticsEvents();
    void updateServerEventsMenu();

    QModelIndex findServerEventsMenuIndexByEventType(
        const QModelIndex& rootIndex,
        const QString& eventType) const;

    bool isFilterExist() const;
    void requestFinished(nx::vms::api::rules::EventLogRecordList&& records);
    void updateActionList(bool instantOnly);

    void query(
        const QnTimePeriod& period,
        const QString& eventType,
        const nx::vms::api::analytics::EventTypeId& analyticsEventTypeId,
        const QString& actionType,
        const QString& text);

private:
    QScopedPointer<Ui::EventLogDialog> ui;

    EventLogModel* m_model;
    QStandardItemModel* m_eventTypesModel;
    QStandardItemModel* m_actionTypesModel;

    rest::Handle m_request = {};

    QnUuidSet m_eventDevices;
    bool m_updateDisabled = false;
    bool m_dirty = false;

    QAction* m_filterAction;
    QAction* m_resetFilterAction;
    QAction* m_selectAllAction;
    QAction* m_exportAction;
    QAction* m_clipboardAction;
    Qt::MouseButton m_lastMouseButton = Qt::NoButton;

    QTimer m_delayUpdateTimer;
};

} // namespace nx::vms::client::desktop
