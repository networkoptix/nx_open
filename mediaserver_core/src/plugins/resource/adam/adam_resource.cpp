#include "adam_resource.h"

#include <functional>
#include <memory>

#include <business/business_event_rule.h>
#include <nx_ec/dummy_handler.h>

#include "api/app_server_connection.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "utils/common/synctime.h"
#include "rest/server/rest_connection_processor.h"
#include "common/common_module.h"
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"

const QString QnAdamResource::kManufacture(lit("Advantech"));

QnAdamResource::QnAdamResource()
{

}

QnAdamResource::~QnAdamResource()
{

}

QString QnAdamResource::getDriverName() const
{
    return kManufacture;
}

void QnAdamResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

CameraDiagnostics::Result QnAdamResource::initInternal()
{
    QnPhysicalCameraResource::initInternal();

    fetchIOPorts();

    return CameraDiagnostics::NoErrorResult();
}

bool QnAdamResource::startInputPortMonitoringAsync(
    std::function<void(bool)>&& completionHandler)
{

    QN_UNUSED(completionHandler);

    if (m_inputsMonitored)
        return true;

    bool shouldNotStartMonitoring = hasFlags(Qn::foreigner)
        || !hasCameraCapabilities(Qn::RelayInputCapability);

    if (shouldNotStartMonitoring)
        return false;

    Qt::connect(
        &m_modbusAsyncClient, &m_modbusAsyncClient::done,
        this, &routeMonitoringFlow);

    Qt::connect(
        &m_modbusAsyncClient, &m_modbusAsyncClient::error,
        this, &handleMonitoringError);


    return requestInputValues();
}

bool QnAdamResource::requestInputValues()
{
    auto startCoil = 0;
    auto endCoil = 0;
    return m_modbusAsyncClient.readCoilsAsync(startCoil, endCoil);
}

void QnAdamResource::stopInputPortMonitoringAsync()
{
    m_inputsMonitored = false;
}

bool QnAdamResource::isInputPortMonitored() const
{
    return m_inputsMonitored;
}

void QnAdamResource::fetchIOPorts()
{
    auto resData = qnCommon->dataPool()->data(toSharedPointer(this));

    auto ioPorts = resData.value<QnIOPortDataList>(Qn::IO_SETTINGS_PARAM_NAME);

    for (const auto& ioPort: ioPorts)
    {
        if (ioPort.portType == Qn::PT_Input)
            m_inputs.push_back(ioPort);
        else if (ioPort.portType == Qn::PT_Output)
            m_outputs.push_back(ioPort);
    }
}

QnIOPortDataList QnAdamResource::getRelayOutputList() const
{
    return m_outputs;
}

QnIOPortDataList QnAdamResource::getInputPortList() const
{
    return m_inputs;
}

bool QnAdamResource::setRelayOutputState(
    const QString& outputID,
    bool isActive,
    unsigned int autoResetTimeoutMS )
{
    qDebug() << lit("Setting relay %1 state, isActive: %2")
        .arg(outputID)
        .arg(isActive);

    return true;
}
