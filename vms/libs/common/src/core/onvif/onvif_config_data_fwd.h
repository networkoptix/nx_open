#ifndef QN_ONVIF_CONFIG_DATA_FWD_H
#define QN_ONVIF_CONFIG_DATA_FWD_H

#include <QtCore/QSharedPointer>
#include <nx/fusion/model_functions_fwd.h>

struct QnOnvifConfigData;
typedef QSharedPointer<QnOnvifConfigData> QnOnvifConfigDataPtr;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnOnvifConfigDataPtr), (json))

#endif // QN_ONVIF_CONFIG_DATA_FWD_H
