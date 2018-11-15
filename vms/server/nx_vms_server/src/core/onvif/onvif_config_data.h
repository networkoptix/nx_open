#ifndef QN_ONVIF_CONFIG_DATA_H
#define QN_ONVIF_CONFIG_DATA_H

#include <QtCore/QMetaType>

#include <common/common_globals.h>

#include "onvif_config_data_fwd.h"

struct QnOnvifConfigData
{
    QnOnvifConfigData() {}

    QVector<QString> videoEncoders;
    QVector<QString> audioEncoders;
    QVector<QString> profiles;
    QVector<QString> ptzConfigurations;
    QVector<QString> ptzProfiles;
};
#define QnOnvifConfigData_Fields (videoEncoders)(audioEncoders)(profiles)\
    (ptzConfigurations)(ptzProfiles)

Q_DECLARE_METATYPE(QnOnvifConfigDataPtr)

#endif // QN_PTZ_DATA_H
