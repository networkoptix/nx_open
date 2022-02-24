// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>
#include <QtCore/QList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/common/ptz/types_fwd.h>

class QnPtzMapper;
typedef QSharedPointer<QnPtzMapper> QnPtzMapperPtr;

class QnAbstractPtzController;
typedef QSharedPointer<QnAbstractPtzController> QnPtzControllerPtr;

class QnProxyPtzController;
typedef QSharedPointer<QnProxyPtzController> QnProxyPtzControllerPtr;

struct QnPtzPreset;
typedef QList<QnPtzPreset> QnPtzPresetList;

struct QnPtzTourSpot;
typedef QList<QnPtzTourSpot> QnPtzTourSpotList;

struct QnPtzTour;
typedef QList<QnPtzTour> QnPtzTourList;

class QnPtzAuxiliaryTrait;
typedef QList<QnPtzAuxiliaryTrait> QnPtzAuxiliaryTraitList;

struct QnPtzLimits;

struct QnPtzData;

struct QnPtzObject;

QN_FUSION_DECLARE_FUNCTIONS(QnPtzLimits, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzTourSpot, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzTour, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzData, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzObject, (json), NX_VMS_COMMON_API)

QN_FUSION_DECLARE_FUNCTIONS(QnPtzAuxiliaryTrait, (json)(lexical), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzMapperPtr, (json), NX_VMS_COMMON_API)
