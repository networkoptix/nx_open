#ifndef CAMERA_OUTPUT_BUSINESS_ACTION_H
#define CAMERA_OUTPUT_BUSINESS_ACTION_H

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <business/business_resource_validator.h>
#include <business/actions/abstract_business_action.h>

#include <core/resource/resource_fwd.h>

class QnCameraOutputBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnCameraOutputBusinessAction(bool instant, const QnBusinessEventParameters &runtimeParams);

    QString getRelayOutputId() const;
    int getRelayAutoResetTimeout() const;

    QString getExternalUniqKey() const;
};

typedef QSharedPointer<QnCameraOutputBusinessAction> QnCameraOutputBusinessActionPtr;

class QnCameraOutputAllowedPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnCameraOutputAllowedPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static inline bool emptyListIsValid() { return false; }
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getErrorText(int invalid, int total);
};

typedef QnBusinessResourceValidator<QnCameraOutputAllowedPolicy> QnCameraOutputValidator;

#endif // CAMERA_OUTPUT_BUSINESS_ACTION_H
