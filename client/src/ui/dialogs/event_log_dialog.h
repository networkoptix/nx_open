#ifndef __EVENT_LOG_DIALOG_H__
#define __EVENT_LOG_DIALOG_H__

#include <QDialog>
#include <QStandardItem>
#include <QAbstractItemModel>
#include <QModelIndex>

#include "business/actions/abstract_business_action.h"
#include "business/events/abstract_business_event.h"
#include "core/resource/network_resource.h"

class QnEventLogModel;
class QnWorkbenchContext;

namespace Ui {
    class EventLogDialog;
}

class QnEventLogDialog: public QDialog
{
    Q_OBJECT

public:
    explicit QnEventLogDialog(QWidget *parent, QnWorkbenchContext *context);
    virtual ~QnEventLogDialog();

    /*
    * Get data from media server
    * \param fromMsec start date. UTC msecs
    * \param toMsec end date. UTC msecs. Can be DATETIME_NOW
    * \param camRes optional camera resource
    * \param businessRuleId optional business rule id
    */
    void query(qint64 fromMsec, qint64 toMsec, 
               QnResourceList camList,
               BusinessEventType::Value eventType, 
               BusinessActionType::Value actionType,
               QnId businessRuleId);
protected:
    void setVisible(bool value) override;
private slots:
    void updateData();
    void at_gotEvents(int httpStatus, const QnLightBusinessActionVectorPtr& events, int requestNum);
    void at_itemClicked(const QModelIndex & index);
    void at_customContextMenuRequested(const QPoint& screenPos);
    void at_cameraButtonClicked();
    void at_filterAction();
    void at_resetFilterAction();
    void at_copyToClipboard();
private:
    QList<QnMediaServerResourcePtr> getServerList() const;
    QString getTextForNCameras(int n) const;
    QStandardItem* createEventTree(QStandardItem* rootItem, BusinessEventType::Value value);

    void setCameraList(QnResourceList resList);
    void setActionType(BusinessActionType::Value value);
    void setEventType(BusinessEventType::Value value);
    bool setEventTypeRecursive(BusinessEventType::Value value, QAbstractItemModel* model, const QModelIndex& parentItem);

    void disableUpdateData();
    void enableUpdateData();
    void updateHeaderWidth();
    bool isFilterExist() const;
    void requestFinished();
private:
    Q_DISABLE_COPY(QnEventLogDialog)
 
    QScopedPointer<Ui::EventLogDialog> ui;
    QnEventLogModel *m_model;
    QSet<int> m_requests;
    QnWorkbenchContext* m_context;

    QVector <QnLightBusinessActionVectorPtr> m_allEvents;
    QnResourceList m_filterCameraList;
    bool m_updateDisabled;
    bool m_dirty;

    QAction* m_filterAction;
    QAction* m_resetFilterAction;
    QAction* m_clipboardAction;

};

#endif // __EVENT_LOG_DIALOG_H____
