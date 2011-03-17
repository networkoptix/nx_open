#include "settings_getter.h"
#include "widgets.h"
#include "device/device.h"
#include "base/sleep.h"

CLDeviceGetParamCommand::CLDeviceGetParamCommand(CLAbstractSettingsWidget* wgt):
CLDeviceCommand(wgt->getDevice()),
m_wgt(wgt)
{
    connect(this, SIGNAL(ongetvalue(QString)), m_wgt, SLOT(updateParam(QString)));// , Qt::BlockingQueuedConnection );
}

void CLDeviceGetParamCommand::execute()
{
    QString name = m_wgt->param().name;
    CLValue val;  
    if (m_device->getParam(name, val, true))
    {
        emit ongetvalue(QString(val));
    }
   
}
