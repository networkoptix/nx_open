#pragma once

#include <QtWidgets/QDialog>
#include <QtGui/QStandardItem>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <api/server_rest_connection_fwd.h>

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/session_aware_dialog.h>

#include <utils/common/connective.h>

class QnEventLogModel;
namespace nx { namespace vms { namespace event { class StringsHelper; }}}

namespace Ui {
    class EventLogDialog;
}

class QnEventLogDialog: public Connective<QnSessionAwareDialog>
{
    Q_OBJECT

    using base_type = Connective<QnSessionAwareDialog>;

public:
    explicit QnEventLogDialog(QWidget *parent);
    virtual ~QnEventLogDialog();

    void disableUpdateData();
    void enableUpdateData();
    void setDateRange(qint64 startTimeMs, qint64 endTimeMs);
    void setCameraList(const QSet<QnUuid>& ids);
    void setActionType(nx::vms::event::ActionType value);
    void setEventType(nx::vms::event::EventType value);

protected:
    void setVisible(bool value) override;
    virtual void retranslateUi() override;

private:
    void initEventsModel();

    void reset();
    void updateData();
    void at_eventsGrid_clicked(const QModelIndex & index);
    void at_eventsGrid_customContextMenuRequested(const QPoint& screenPos);
    void at_cameraButton_clicked();
    void at_filterAction_triggered();
    void at_clipboardAction_triggered();
    void at_exportAction_triggered();
    void at_mouseButtonRelease(QObject* sender, QEvent* event);

    QStandardItem* createEventTree(QStandardItem* rootItem, nx::vms::event::EventType value);
    void createAnalyticsEventTree(QStandardItem* rootItem);
    void updateAnalyticsEvents();

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
        nx::vms::event::EventType eventType,
        const QnUuid& eventSubtype,
        nx::vms::event::ActionType actionType);


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
};
