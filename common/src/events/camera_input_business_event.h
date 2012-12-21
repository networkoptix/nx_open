/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef CAMERA_INPUT_BUSINESS_EVENT_H
#define CAMERA_INPUT_BUSINESS_EVENT_H

#include "prolonged_business_event.h"

namespace BusinessEventParameters
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

    virtual QString toString() const;

    const QString& inputPortID() const;

    virtual bool checkCondition(const QnBusinessParams &params) const override;

private:
    const QString m_inputPortID;
};

typedef QSharedPointer<QnCameraInputEvent> QnCameraInputEventPtr;

#endif //CAMERA_INPUT_BUSINESS_EVENT_H
