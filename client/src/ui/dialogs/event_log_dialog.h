#ifndef QN_EVENT_LOG_DIALOG_H
#define QN_EVENT_LOG_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtGui/QStandardItem>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QnEventLogModel;
class QnBusinessRuleViewModel;

namespace Ui {
    class EventLogDialog;
}

class QnEventLogDialog: public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    explicit QnEventLogDialog(QWidget *parent);
    virtual ~QnEventLogDialog();

    void disableUpdateData();
    void enableUpdateData();
    void setDateRange(const QDate& from, const QDate& to);
    void setCameraList(const QnResourceList &cameras);
    void setActionType(QnBusiness::ActionType value);
    void setEventType(QnBusiness::EventType value);

protected:
    void setVisible(bool value) override;

private slots:
    void updateData();
    void at_gotEvents(int httpStatus, const QnBusinessActionDataListPtr& events, int requestNum);
    void at_eventsGrid_clicked(const QModelIndex & index);
    void at_eventsGrid_customContextMenuRequested(const QPoint& screenPos);
    void at_cameraButton_clicked();
    void at_filterAction_triggered();
    void at_resetFilterAction_triggered();
    void at_clipboardAction_triggered();
    void at_exportAction_triggered();
    void at_mouseButtonRelease(QObject* sender, QEvent* event);

private:
    QList<QnMediaServerResourcePtr> getServerList() const;
    QString getTextForNCameras(int n) const;
    QStandardItem* createEventTree(QStandardItem* rootItem, QnBusiness::EventType value);

    void updateHeaderWidth();
    bool isFilterExist() const;
    void requestFinished();
    bool isCameraMatched(QnBusinessRuleViewModel* ruleModel) const;
    void updateActionList(bool instantOnly);

    /**
     * Get data from server
     * 
     * \param fromMsec start date. UTC msecs
     * \param toMsec end date. UTC msecs. Can be DATETIME_NOW
     */
    void query(qint64 fromMsec, qint64 toMsec, QnBusiness::EventType eventType, QnBusiness::ActionType actionType);

private:
    QScopedPointer<Ui::EventLogDialog> ui;

    QnEventLogModel *m_model;
    QStandardItemModel *m_eventTypesModel;
    QStandardItemModel *m_actionTypesModel;

    QSet<int> m_requests;

    QVector <QnBusinessActionDataListPtr> m_allEvents;
    QnResourceList m_filterCameraList;
    bool m_updateDisabled;
    bool m_dirty;

    QAction *m_filterAction;
    QAction *m_resetFilterAction;
    QAction *m_selectAllAction;
    QAction *m_exportAction;
    QAction *m_clipboardAction;
    Qt::MouseButton m_lastMouseButton;
};

#endif // QN_EVENT_LOG_DIALOG_H
