#ifndef __RECORDING_BUSINESS_ACTION_H__
#define __RECORDING_BUSINESS_ACTION_H__

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <business/business_resource_validator.h>
#include <business/actions/abstract_business_action.h>

#include <core/resource/resource_fwd.h>

class QnRecordingBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnRecordingBusinessAction(const QnBusinessEventParameters &runtimeParams);

    int getFps() const;
    Qn::StreamQuality getStreamQuality() const;
    int getRecordDuration() const;
    int getRecordBefore() const;
    int getRecordAfter() const;
};

typedef QSharedPointer<QnRecordingBusinessAction> QnRecordingBusinessActionPtr; // TODO: #Elric move to fwd header.

class QnCameraRecordingAllowedPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnCameraRecordingAllowedPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static inline bool emptyListIsValid() { return false; }
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getErrorText(int invalid, int total);
};

typedef QnBusinessResourceValidator<QnCameraRecordingAllowedPolicy> QnCameraRecordingValidator;

#endif // __RECORDING_BUSINESS_ACTION_H__
