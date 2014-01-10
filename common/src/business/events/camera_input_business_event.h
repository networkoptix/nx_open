/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef CAMERA_INPUT_BUSINESS_EVENT_H
#define CAMERA_INPUT_BUSINESS_EVENT_H

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <business/business_resource_validator.h>
#include <business/events/prolonged_business_event.h>

#include <core/resource/resource_fwd.h>

class QnCameraInputEvent: public QnProlongedBusinessEvent {
    typedef QnProlongedBusinessEvent base_type;
public:
    QnCameraInputEvent(const QnResourcePtr& resource, Qn::ToggleState toggleState, qint64 timeStamp, const QString& inputPortID);

    const QString& inputPortID() const;

    virtual bool checkCondition(Qn::ToggleState state, const QnBusinessEventParameters &params) const override;

    virtual QnBusinessEventParameters getRuntimeParams() const override;
private:
    const QString m_inputPortID;
};

typedef QSharedPointer<QnCameraInputEvent> QnCameraInputEventPtr;

class QnCameraInputAllowedPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnCameraInputAllowedPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static inline bool emptyListIsValid() { return true; }
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getErrorText(int invalid, int total);
};

typedef QnBusinessResourceValidator<QnCameraInputAllowedPolicy> QnCameraInputValidator;


#endif //CAMERA_INPUT_BUSINESS_EVENT_H
