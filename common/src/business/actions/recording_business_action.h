#ifndef __RECORDING_BUSINESS_ACTION_H__
#define __RECORDING_BUSINESS_ACTION_H__

#include "abstract_business_action.h"

#include <core/resource/media_resource.h>
#include <core/resource/resource_fwd.h>

class QnRecordingBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnRecordingBusinessAction(const QnBusinessEventParameters &runtimeParams);

    int getFps() const;
    QnStreamQuality getStreamQuality() const;
    int getRecordDuration() const;
    int getRecordBefore() const;
    int getRecordAfter() const;

    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static bool isResourcesListValid(const QnResourceList &resources); // TODO: #Elric move out, generalize
    static int  invalidResourcesCount(const QnResourceList &resources);
};

typedef QSharedPointer<QnRecordingBusinessAction> QnRecordingBusinessActionPtr;

#endif // __RECORDING_BUSINESS_ACTION_H__
