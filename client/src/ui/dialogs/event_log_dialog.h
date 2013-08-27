#ifndef __EVENT_LOG_DIALOG_H__
#define __EVENT_LOG_DIALOG_H__

#include <QDialog>
#include <QStandardItem>
#include <QAbstractItemModel>
#include <QModelIndex>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/button_box_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

class QnEventLogModel;
class QnBusinessRuleViewModel;

namespace Ui {
    class EventLogDialog;
}

class QnEventLogDialog: public QnButtonBoxDialog, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnEventLogDialog(QWidget *parent, QnWorkbenchContext *context);
    virtual ~QnEventLogDialog();

    void disableUpdateData();
    void enableUpdateData();
    void setDateRange(const QDate& from, const QDate& to);
    void setCameraList(const QnResourceList &cameras);
    void setActionType(BusinessActionType::Value value);
    void setEventType(BusinessEventType::Value value);
protected:
    void setVisible(bool value) override;
private slots:
    void updateData();
    void at_gotEvents(int httpStatus, const QnBusinessActionDataListPtr& events, int requestNum);
    void at_itemClicked(const QModelIndex & index);
    void at_customContextMenuRequested(const QPoint& screenPos);
    void at_cameraButtonClicked();
    void at_filterAction();
    void at_resetFilterAction();
    void at_selectAllAction();
    void at_copyToClipboard();
    void at_exportAction();
    void at_mouseButtonRelease(QObject* sender, QEvent* event);
private:
    QList<QnMediaServerResourcePtr> getServerList() const;
    QString getTextForNCameras(int n) const;
    QStandardItem* createEventTree(QStandardItem* rootItem, BusinessEventType::Value value);

    void updateHeaderWidth();
    bool isFilterExist() const;
    void requestFinished();
    bool isCameraMatched(QnBusinessRuleViewModel* ruleModel) const;
    void updateActionList(bool instantOnly);

    /**
    * Get data from media server
    * \param fromMsec start date. UTC msecs
    * \param toMsec end date. UTC msecs. Can be DATETIME_NOW
    * \param camRes optional camera resource
    */
    void query(qint64 fromMsec, qint64 toMsec,
               BusinessEventType::Value eventType,
               BusinessActionType::Value actionType);
private:
    Q_DISABLE_COPY(QnEventLogDialog)

    QScopedPointer<Ui::EventLogDialog> ui;

    QnEventLogModel *m_model;
    QStandardItemModel *m_eventTypesModel;
    QStandardItemModel *m_actionTypesModel;

    QSet<int> m_requests;

    QVector <QnBusinessActionDataListPtr> m_allEvents;
    QnResourceList m_filterCameraList;
    bool m_updateDisabled;
    bool m_dirty;

    QAction* m_filterAction;
    QAction* m_resetFilterAction;
    QAction* m_selectAllAction;
    QAction* m_exportAction;
    QAction* m_clipboardAction;
    Qt::MouseButton m_lastMouseButton;
};

#endif // __EVENT_LOG_DIALOG_H____
