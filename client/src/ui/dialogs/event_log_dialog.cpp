#include "event_log_dialog.h"
#include "ui_event_log_dialog.h"
#include "ui/models/event_log_model.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_managment/resource_pool.h"
#include "ui/workbench/workbench_context.h"

QnEventLogDialog::QnEventLogDialog(QWidget *parent, QnWorkbenchContext *context):
    QDialog(parent),
    ui(new Ui::EventLogDialog),
    m_context(context)
{
    ui->setupUi(this);

    QList<QnEventLogModel::Column> columns;
        columns << QnEventLogModel::DateTimeColumn << QnEventLogModel::EventColumn << QnEventLogModel::EventCameraColumn <<
        QnEventLogModel::ActionColumn << QnEventLogModel::ActionCameraColumn << QnEventLogModel::RepeatCountColumn << QnEventLogModel::DescriptionColumn;

    m_model = new QnEventLogModel(this);
    m_model->setColumns(columns);
    ui->gridEvents->setModel(m_model);

    query(0, DATETIME_NOW);
}

QnEventLogDialog::~QnEventLogDialog()
{

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

void QnEventLogDialog::query(qint64 fromMsec, qint64 toMsec, QnNetworkResourcePtr camRes, QnId businessRuleId)
{
    m_model->clear();
    m_requests.clear();

    QList<QnMediaServerResourcePtr> mediaServerList = getServerList();
    foreach(const QnMediaServerResourcePtr& mserver, mediaServerList)
    {
        m_requests << mserver->apiConnection()->asyncEventLog(camRes, fromMsec, toMsec, businessRuleId, this, SLOT(at_gotEvents(int, int, const QnAbstractBusinessActionList&)));
    }

}

void QnEventLogDialog::at_gotEvents(int requestNum, int httpStatus, const QnAbstractBusinessActionList& events)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);
    if (!events.isEmpty())
        m_model->addEvents(events);
}
