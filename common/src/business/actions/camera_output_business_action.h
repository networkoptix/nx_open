#ifndef CAMERA_OUTPUT_BUSINESS_ACTION_H
#define CAMERA_OUTPUT_BUSINESS_ACTION_H

#include "abstract_business_action.h"

namespace BusinessActionParameters {

    QString getRelayOutputId(const QnBusinessParams &params);
    void setRelayOutputId(QnBusinessParams* params, const QString &value);

    int getRelayAutoResetTimeout(const QnBusinessParams &params);
    void setRelayAutoResetTimeout(QnBusinessParams* params, int value);
}

class QnCameraOutputBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnCameraOutputBusinessAction(const QnBusinessParams &runtimeParams);

    QString getRelayOutputId() const;
    int getRelayAutoResetTimeout() const;

    QString getExternalUniqKey() const;
};

typedef QSharedPointer<QnCameraOutputBusinessAction> QnCameraOutputBusinessActionPtr;

#endif // CAMERA_OUTPUT_BUSINESS_ACTION_H
