#ifndef __EVENT_LOG_DIALOG_H__
#define __EVENT_LOG_DIALOG_H__

#include <QDialog>
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
private slots:
    void updateData();
    void at_gotEvents(int httpStatus, const QnLightBusinessActionVectorPtr& events, int requestNum);
    void onItemClicked(QListWidgetItem * item);
    void at_customContextMenuRequested(const QPoint& screenPos);
    void at_cameraButtonClicked();
private:
    QList<QnMediaServerResourcePtr> getServerList() const;
    QAction* getFilterAction(const QMenu* menu, const QModelIndex& idx);
    QString getTextForNCameras(int n) const;
    QStandardItem* createEventItem(BusinessEventType::Value value);
private:
    Q_DISABLE_COPY(QnEventLogDialog)
 
    QScopedPointer<Ui::EventLogDialog> ui;
    QnEventLogModel *m_model;
    QSet<int> m_requests;
    QnWorkbenchContext* m_context;

    QList <QnLightBusinessActionVectorPtr> m_allEvents;
    QnResourceList m_filterCameraList;
};

#endif // __EVENT_LOG_DIALOG_H____