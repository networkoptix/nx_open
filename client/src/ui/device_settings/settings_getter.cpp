#include "settings_getter.h"

#include "widgets.h"

QnDeviceGetParamCommand::QnDeviceGetParamCommand(CLAbstractSettingsWidget *wgt)
    : QnResourceCommand(wgt->resource()),
      m_wgt(wgt)
{
    connect(this, SIGNAL(ongetvalue(QString)), m_wgt, SLOT(updateParam(QString)));// , Qt::BlockingQueuedConnection);
}

void QnDeviceGetParamCommand::execute()
{
    if (!isConnectedToTheResource())
        return;

    QVariant val;
    if (m_resource->getParam(m_wgt->param().name(), val, QnDomainPhysical))
        Q_EMIT ongetvalue(val.toString());
}
