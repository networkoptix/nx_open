/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef CAMERA_INPUT_BUSINESS_EVENT_H
#define CAMERA_INPUT_BUSINESS_EVENT_H

#include <business/events/prolonged_business_event.h>

#include <core/resource/resource_fwd.h>

namespace QnBusinessEventRuntime
{
    QString getInputPortId(const QnBusinessParams &params);
    void setInputPortId(QnBusinessParams* params, const QString &value);
}

class QnCameraInputEvent: public QnProlongedBusinessEvent
{
    typedef QnProlongedBusinessEvent base_type;
public:
    QnCameraInputEvent(
        const QnResourcePtr& resource,
        ToggleState::Value toggleState,
        qint64 timeStamp,
        const QString& inputPortID);

    const QString& inputPortID() const;

    virtual bool checkCondition(ToggleState::Value state, const QnBusinessParams &params) const override;

    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static bool isResourcesListValid(const QnResourceList &resources);

    virtual QnBusinessParams getRuntimeParams() const override;
private:
    const QString m_inputPortID;
};

typedef QSharedPointer<QnCameraInputEvent> QnCameraInputEventPtr;

#endif //CAMERA_INPUT_BUSINESS_EVENT_H
