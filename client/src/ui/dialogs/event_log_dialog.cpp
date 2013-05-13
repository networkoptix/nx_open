#include "event_log_dialog.h"
#include "ui_event_log_dialog.h"
#include "ui/models/event_log_model.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_managment/resource_pool.h"
#include "ui/workbench/workbench_context.h"
#include "business/events/abstract_business_event.h"
#include "ui/actions/action_manager.h"
#include "ui/actions/actions.h"

QnEventLogDialog::QnEventLogDialog(QWidget *parent, QnWorkbenchContext *context):
    QDialog(parent),
    ui(new Ui::EventLogDialog),
    m_context(context)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    QList<QnEventLogModel::Column> columns;
        columns << QnEventLogModel::DateTimeColumn << QnEventLogModel::EventColumn << QnEventLogModel::EventCameraColumn <<
        QnEventLogModel::ActionColumn << QnEventLogModel::ActionCameraColumn << QnEventLogModel::DescriptionColumn;

    m_model = new QnEventLogModel(this);
    m_model->setColumns(columns);
    ui->gridEvents->setModel(m_model);

    QDate dt = QDateTime::currentDateTime().date();
    ui->dateEditFrom->setDate(dt);
    ui->dateEditTo->setDate(dt);

    ui->gridEvents->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);

    QStringList eventItems;
    eventItems << tr("All events");
    for (int i = 0; i < (int) BusinessEventType::NotDefined; ++i)
        eventItems << BusinessEventType::toString(BusinessEventType::Value(i));
    ui->eventComboBox->addItems(eventItems);

    QStringList actionItems;
    actionItems << tr("All actions");
    for (int i = 0; i < (int) BusinessActionType::NotDefined; ++i)
        actionItems << BusinessActionType::toString(BusinessActionType::Value(i));
    ui->actionComboBox->addItems(actionItems);

    connect(ui->dateEditFrom, SIGNAL(dateChanged(const QDate &)), this, SLOT(updateData()) );
    connect(ui->dateEditTo, SIGNAL(dateChanged(const QDate &)), this, SLOT(updateData()) );

    connect(ui->eventComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateData()) );
    connect(ui->actionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateData()) );

    //connect(ui->gridEvents->selectionModel(), SIGNAL(currentChanged(const QModelIndex&,const QModelIndex&)), this, SLOT(at_gridCelLSelected(const QModelIndex&)) );

    connect(ui->gridEvents, SIGNAL(customContextMenuRequested (const QPoint &)), this, SLOT(at_customContextMenuRequested(const QPoint &)) );


    updateData();
}

QnEventLogDialog::~QnEventLogDialog()
{
}

void QnEventLogDialog::updateData()
{
    BusinessEventType::Value eventType = BusinessEventType::NotDefined;
    BusinessActionType::Value actionType = BusinessActionType::NotDefined;

    if (ui->eventComboBox->currentIndex() > 0)
        eventType = BusinessEventType::Value(ui->eventComboBox->currentIndex()-1);
    if (ui->actionComboBox->currentIndex() > 0)
        actionType = BusinessActionType::Value(ui->actionComboBox->currentIndex()-1);

    query(ui->dateEditFrom->dateTime().toMSecsSinceEpoch(), ui->dateEditTo->dateTime().addDays(1).toMSecsSinceEpoch(),
          QnNetworkResourcePtr(), // todo: add camera resource here
          eventType, actionType,
          QnId() // todo: add businessRuleID here
          );

    ui->gridEvents->setDisabled(true);
    ui->groupBox->setCursor(Qt::WaitCursor);
}

QList<QnMediaServerResourcePtr> QnEventLogDialog::getServerList() const
{
    QList<QnMediaServerResourcePtr> result;
    QnResourceList resList = m_context->resourcePool()->getAllResourceByTypeName(lit("Server"));
    foreach(const QnResourcePtr& r, resList) {
        QnMediaServerResourcePtr mServer = r.dynamicCast<QnMediaServerResource>();
        if (mServer)
            result << mServer;
    }

    return result;
}

void QnEventLogDialog::query(qint64 fromMsec, qint64 toMsec, 
                             QnNetworkResourcePtr camRes, 
                             BusinessEventType::Value eventType,
                             BusinessActionType::Value actionType,
                             QnId businessRuleId)
{
    m_requests.clear();
    m_allEvents.clear();
    

    QList<QnMediaServerResourcePtr> mediaServerList = getServerList();
    foreach(const QnMediaServerResourcePtr& mserver, mediaServerList)
    {
        if (mserver->getStatus() == QnResource::Online) 
        {
            m_requests << mserver->apiConnection()->asyncEventLog(
                fromMsec, toMsec, 
                camRes, 
                eventType,
                actionType,
                businessRuleId, 
                this, SLOT(at_gotEvents(int, const QnLightBusinessActionVectorPtr&, int)));
        }
    }

}

QnLightBusinessActionVectorPtr QnEventLogDialog::merge2(const QList <QnLightBusinessActionVectorPtr>& eventsList) const
{
    QnLightBusinessActionVectorPtr events1 = eventsList[0];
    QnLightBusinessActionVectorPtr events2 = eventsList[1];

    QnLightBusinessActionVectorPtr events = QnLightBusinessActionVectorPtr(new QnLightBusinessActionVector);
    events->resize(events1->size() + events2->size());

    int idx1 = 0, idx2 = 0;

    QnLightBusinessActionVector& v1  = *events1.data();
    QnLightBusinessAction* ptr1 = &v1[0];

    QnLightBusinessActionVector& v2  = *events2.data();
    QnLightBusinessAction* ptr2 = &v2[0];

    QnLightBusinessActionVector& vDst = *events.data();
    QnLightBusinessAction* dst  = &vDst[0];


    while (idx1 < events1->size() && idx2 < events2->size())
    {
        if (ptr1[idx1].timestamp() <= ptr2[idx2].timestamp())
            *dst++ = ptr1[idx1++];
        else
            *dst++ = ptr2[idx2++];
    }

    while (idx1 < events1->size())
        *dst++ = ptr1[idx1++];

    while (idx2 < events2->size())
        *dst++ = ptr2[idx2++];

    return events;
}

QnLightBusinessActionVectorPtr QnEventLogDialog::mergeN(const QList <QnLightBusinessActionVectorPtr>& eventsList) const
{
    QnLightBusinessActionVectorPtr events = QnLightBusinessActionVectorPtr(new QnLightBusinessActionVector);

    QVector<int> idx;
    idx.resize(eventsList.size());
    
    QVector<QnLightBusinessAction*> ptr;
    QVector<QnLightBusinessAction*> ptrEnd;
    int totalSize = 0;
    for (int i = 0; i < eventsList.size(); ++i)
    {
        QnLightBusinessActionVector& tmp  = *eventsList[i].data();
        ptr << &tmp[0];
        ptrEnd << &tmp[0] + eventsList[i]->size();
        totalSize += eventsList[i]->size();
    }
    events->resize(totalSize);
    QnLightBusinessActionVector& vDst = *events.data();
    QnLightBusinessAction* dst  = &vDst[0];

    int curIdx = -1;
    do
    {
        qint64 curTimestamp = DATETIME_NOW;
        curIdx = -1;
        for (int i = 0; i < eventsList.size(); ++i)
        {
            if (ptr[i] < ptrEnd[i] && ptr[i]->timestamp() < curTimestamp) {
                curIdx = i;
                curTimestamp = ptr[i]->timestamp();
            }
        }
        if (curIdx >= 0) {
            *dst++ = *ptr[curIdx];
            ptr[curIdx]++;
        }
    } while (curIdx >= 0);

    return events;
}

QnLightBusinessActionVectorPtr QnEventLogDialog::mergeEvents(const QList <QnLightBusinessActionVectorPtr>& eventsList) const
{
    if (eventsList.empty())
        return QnLightBusinessActionVectorPtr();
    else if (eventsList.size() == 1)
        return eventsList[0];
    else if (eventsList.size() == 2)
        return merge2(eventsList);
    else
        return mergeN(eventsList);
}

void QnEventLogDialog::at_gotEvents(int httpStatus, const QnLightBusinessActionVectorPtr& events, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);
    if (!events->empty())
        m_allEvents << events;
    if (m_requests.isEmpty()) 
    {
        m_model->setEvents(mergeEvents(m_allEvents));
        m_allEvents.clear();
        ui->gridEvents->setDisabled(false);
        ui->groupBox->setCursor(Qt::ArrowCursor);
    }
}

void QnEventLogDialog::onItemClicked(QListWidgetItem * item)
{
}

void QnEventLogDialog::at_customContextMenuRequested(const QPoint& screenPos)
{
    QModelIndex idx = ui->gridEvents->currentIndex();
    QnId resId = m_model->data(ui->gridEvents->currentIndex(), Qn::ResourceRole).toInt();
    if (!resId.isValid())
        return;
    QnResourcePtr resource = qnResPool->getResourceById(resId);
    if (!resource)
        return;

    QnActionManager *manager = m_context->menu();
    const QnActionParameters parameters(resource);
    QScopedPointer<QMenu> menu(manager->newMenu(Qn::ActionScope::TreeScope, parameters));
    if (menu) {
        //manager->redirectAction(menu.data(), Qn::RenameAction, NULL);
        QAction *action = menu->exec(screenPos + pos());
    }
}
