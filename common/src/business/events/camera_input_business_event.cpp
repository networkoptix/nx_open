/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#include "camera_input_business_event.h"
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>


namespace QnBusinessEventRuntime {

    static QLatin1String inputPortId( "inputPortId" );

    QString getInputPortId(const QnBusinessParams &params) {
        return params.value(inputPortId, QString()).toString();
    }

    void setInputPortId(QnBusinessParams *params, const QString &value) {
        (*params)[inputPortId] = value;
    }

}


QnCameraInputEvent::QnCameraInputEvent(
    const QnResourcePtr& resource,
    ToggleState::Value toggleState,
    qint64 timeStamp,
    const QString& inputPortID)
:
    base_type(
        BusinessEventType::Camera_Input,
        resource,
        toggleState,
        timeStamp),
    m_inputPortID( inputPortID )
{
}

const QString& QnCameraInputEvent::inputPortID() const
{
    return m_inputPortID;
}

bool QnCameraInputEvent::checkCondition(ToggleState::Value state, const QnBusinessParams &params) const {
    if (!base_type::checkCondition(state, params))
        return false;

    QString inputPort = QnBusinessEventRuntime::getInputPortId(params);
    return inputPort.isEmpty() || inputPort == m_inputPortID;
}

bool QnCameraInputEvent::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return (camera->getCameraCapabilities() & Qn::RelayInputCapability);
}

bool QnCameraInputEvent::isResourcesListValid(const QnResourceList &resources) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
        return true; // should no check if any camera is selected
    foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
        if (!isResourceValid(camera)) {
            return false;
        }
    }
    return true;
}

QnBusinessParams QnCameraInputEvent::getRuntimeParams() const {
    QnBusinessParams params = base_type::getRuntimeParams();
    QnBusinessEventRuntime::setInputPortId(&params, m_inputPortID);
    QnBusinessEventRuntime::setParamsKey(&params, m_inputPortID);
    return params;
}

