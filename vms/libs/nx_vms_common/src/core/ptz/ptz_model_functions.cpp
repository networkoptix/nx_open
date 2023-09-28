// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/fusion/model_functions.h>

#include "ptz_tour.h"
#include "ptz_preset.h"
#include "ptz_limits.h"
#include "ptz_data.h"
#include "ptz_object.h"

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzLimits, (json), QnPtzLimits_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzTourSpot, (json), QnPtzTourSpot_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzTour, (json), QnPtzTour_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzData, (json), QnPtzData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzObject, (json), QnPtzObject_Fields)
