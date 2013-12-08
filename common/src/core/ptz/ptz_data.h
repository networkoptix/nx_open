#ifndef QN_PTZ_DATA_H
#define QN_PTZ_DATA_H

#include <QtCore/QMetaType>
#include <QtGui/QVector3D>

#include <common/common_globals.h>

#include "ptz_fwd.h"
#include "ptz_limits.h"
#include "ptz_preset.h"
#include "ptz_tour.h"

struct QnPtzData {
    Qn::PtzDataFields fields;
    Qn::PtzCapabilities capabilities;
    QVector3D logicalPosition;
    QVector3D devicePosition;
    QnPtzLimits logicalLimits;
    QnPtzLimits deviceLimits;
    Qt::Orientations flip;
    QnPtzPresetList presets;
    QnPtzTourList tours;
};

Q_DECLARE_METATYPE(QnPtzData)

#endif // QN_PTZ_DATA_H
