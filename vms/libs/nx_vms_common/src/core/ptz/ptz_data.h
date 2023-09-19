// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QVector3D>

#include <core/ptz/ptz_constants.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/vms/common/ptz/datafield.h>
#include <nx/vms/common/ptz/vector.h>

#include "ptz_auxiliary_trait.h"
#include "ptz_fwd.h"
#include "ptz_limits.h"
#include "ptz_object.h"
#include "ptz_preset.h"
#include "ptz_tour.h"

struct QnPtzData
{
    using DataFields = nx::vms::common::ptz::DataFields;
    using Vector = nx::vms::common::ptz::Vector;

    QnPtzData() {}
    QnPtzData(DataFields query, DataFields fields): query(query), fields(fields) {}

    /** Fields that were queried from the underlying PTZ controller to fill this data object. */
    DataFields query;
    /** Fields that are valid in this data object. */
    DataFields fields;
    Ptz::Capabilities capabilities;
    Vector logicalPosition;
    Vector devicePosition;
    QnPtzLimits logicalLimits;
    QnPtzLimits deviceLimits;
    Qt::Orientations flip;
    QnPtzPresetList presets;
    QnPtzTourList tours;
    QnPtzObject activeObject;
    QnPtzObject homeObject;
    QnPtzAuxiliaryTraitList auxiliaryTraits;
};
#define QnPtzData_Fields (query)(fields)(capabilities)(logicalPosition)(devicePosition)\
    (logicalLimits)(deviceLimits)(flip)(presets)(tours)(activeObject)(homeObject)
