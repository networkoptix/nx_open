#ifndef CAMERA_OUTPUT_BUSINESS_ACTION_H
#define CAMERA_OUTPUT_BUSINESS_ACTION_H

#include "abstract_business_action.h"
#include <core/resource/resource_fwd.h>

class QnCameraOutputBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnCameraOutputBusinessAction(bool instant, const QnBusinessParams &runtimeParams);

    QString getRelayOutputId() const;
    int getRelayAutoResetTimeout() const;

    QString getExternalUniqKey() const;

    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static bool isResourcesListValid(const QnResourceList &resources);
    static int  invalidResourcesCount(const QnResourceList &resources);
};

typedef QSharedPointer<QnCameraOutputBusinessAction> QnCameraOutputBusinessActionPtr;

#endif // CAMERA_OUTPUT_BUSINESS_ACTION_H
