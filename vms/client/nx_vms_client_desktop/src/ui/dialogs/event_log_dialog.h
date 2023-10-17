// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>
#include <QtCore/QTimer>
#include <QtGui/QStandardItem>
#include <QtWidgets/QDialog>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <ui/dialogs/common/session_aware_dialog.h>

class QnEventLogModel;
namespace nx { namespace vms { namespace event { class StringsHelper; }}}

namespace Ui {
    class EventLogDialog;
}

class QnEventLogDialog: public QnSessionAwareDialog
{
    Q_OBJECT

    using base_type = QnSessionAwareDialog;

public:
    explicit QnEventLogDialog(QWidget *parent);
    virtual ~QnEventLogDialog();

    void disableUpdateData();
    void enableUpdateData();
    void setDateRange(qint64 startTimeMs, qint64 endTimeMs);
    QnUuidSet getCameraList() const;
    void setCameraList(const QnUuidSet& ids);
    void setActionType(nx::vms::api::ActionType value);
    void setEventType(nx::vms::api::EventType value);
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
    void at_eventsGrid_clicked(const QModelIndex & index);
    void at_eventsGrid_customContextMenuRequested(const QPoint& screenPos);
    void at_cameraButton_clicked();
    void at_filterAction_triggered();
    void at_clipboardAction_triggered();
    void at_exportAction_triggered();
    void at_mouseButtonRelease(QObject* sender, QEvent* event);

    QStandardItem* createEventTree(QStandardItem* rootItem, nx::vms::api::EventType value);
    void createAnalyticsEventTree(QStandardItem* rootItem);
    void updateAnalyticsEvents();
    void updateServerEventsMenu();

    QModelIndex findServerEventsMenuIndexByEventType(const QModelIndex& rootIndex,
        nx::vms::api::EventType eventType) const;

    bool isFilterExist() const;
    void requestFinished();
    void updateActionList(bool instantOnly);

    /**
     * Get data from server
     *
     * \param fromMsec start date. UTC msecs
     * \param toMsec end date. UTC msecs. Can be DATETIME_NOW
     */
    void query(qint64 fromMsec,
        qint64 toMsec,
        nx::vms::api::EventType eventType,
        const nx::vms::api::analytics::EventTypeId& analyticsEventTypeId,
        nx::vms::api::ActionType actionType,
        const QString& text);

private:
    QScopedPointer<Ui::EventLogDialog> ui;

    QnEventLogModel *m_model;
    QStandardItemModel *m_eventTypesModel;
    QStandardItemModel *m_actionTypesModel;

    QSet<rest::Handle> m_requests;

    nx::vms::event::ActionDataList m_allEvents;
    QSet<QnUuid> m_filterCameraList;
    bool m_updateDisabled;
    bool m_dirty;

    QAction *m_filterAction;
    QAction *m_resetFilterAction;
    QAction *m_selectAllAction;
    QAction *m_exportAction;
    QAction *m_clipboardAction;
    Qt::MouseButton m_lastMouseButton;

    std::unique_ptr<nx::vms::event::StringsHelper> m_helper;
    QTimer m_delayUpdateTimer;
};
