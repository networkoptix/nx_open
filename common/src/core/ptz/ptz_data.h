#ifndef QN_PTZ_DATA_H
#define QN_PTZ_DATA_H

#include <QtCore/QMetaType>
#include <QtGui/QVector3D>

#include <common/common_globals.h>
#include <core/ptz/ptz_constants.h>

#include "ptz_fwd.h"
#include "ptz_limits.h"
#include "ptz_preset.h"
#include "ptz_tour.h"
#include "ptz_object.h"
#include "ptz_auxilary_trait.h"

#include <nx/core/ptz/vector.h>

struct QnPtzData {
    QnPtzData() {}
    QnPtzData(Qn::PtzDataFields query, Qn::PtzDataFields fields): query(query), fields(fields) {}

    Qn::PtzDataFields query;    /**< Fields that were queried from the underlying PTZ controller to fill this data object. */
    Qn::PtzDataFields fields;   /**< Fields that are valid in this data object. */
    Ptz::Capabilities capabilities;
    nx::core::ptz::Vector logicalPosition;
    nx::core::ptz::Vector devicePosition;
    QnPtzLimits logicalLimits;
    QnPtzLimits deviceLimits;
    Qt::Orientations flip;
    QnPtzPresetList presets;
    QnPtzTourList tours;
    QnPtzObject activeObject;
    QnPtzObject homeObject;
    QnPtzAuxilaryTraitList auxilaryTraits;
};
#define QnPtzData_Fields (query)(fields)(capabilities)(logicalPosition)(devicePosition)(logicalLimits)(deviceLimits)(flip)(presets)(tours)(activeObject)(homeObject)

Q_DECLARE_METATYPE(QnPtzData)

#endif // QN_PTZ_DATA_H
