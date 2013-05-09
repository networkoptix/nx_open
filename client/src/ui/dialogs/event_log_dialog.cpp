#include "event_log_dialog.h"
#include "ui_event_log_dialog.h"
#include "ui/models/event_log_model.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_managment/resource_pool.h"
#include "ui/workbench/workbench_context.h"
#include "business/events/abstract_business_event.h"

QnEventLogDialog::QnEventLogDialog(QWidget *parent, QnWorkbenchContext *context):
    QDialog(parent),
    ui(new Ui::EventLogDialog),
    m_context(context)
{
    ui->setupUi(this);

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
    m_model->clear();
    m_requests.clear();

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
                this, SLOT(at_gotEvents(int, int, const QnAbstractBusinessActionList&)));
        }
    }

}

void QnEventLogDialog::at_gotEvents(int requestNum, int httpStatus, const QnAbstractBusinessActionList& events)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);
    m_model->addEvents(events);
    if (m_requests.isEmpty())
        m_model->rebuild();
}

void QnEventLogDialog::onItemClicked(QListWidgetItem * item)
{
    QString mylink = item->data(Qt::UserRole).toString();
    QDesktopServices::openUrl(QUrl(mylink));
}
