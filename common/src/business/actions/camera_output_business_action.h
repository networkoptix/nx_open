#ifndef CAMERA_OUTPUT_BUSINESS_ACTION_H
#define CAMERA_OUTPUT_BUSINESS_ACTION_H

#include <business/actions/abstract_business_action.h>

#include <core/resource/resource_fwd.h>

class QnCameraOutputBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnCameraOutputBusinessAction(bool instant, const QnBusinessEventParameters &runtimeParams);

    static const int kInstantActionAutoResetTimeoutMs = 30000;

    QString getRelayOutputId() const;
    int getRelayAutoResetTimeout() const;

    QString getExternalUniqKey() const;
};

typedef QSharedPointer<QnCameraOutputBusinessAction> QnCameraOutputBusinessActionPtr;

#endif // CAMERA_OUTPUT_BUSINESS_ACTION_H
