#pragma once

#include <QtCore/QSharedPointer>
#include <QtCore/QList>

#include <nx/fusion/model_functions_fwd.h>

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

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (QnPtzLimits)(QnPtzTourSpot)(QnPtzTour)(QnPtzData)(QnPtzObject),
    (json)(eq))

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnPtzAuxiliaryTrait), (json)(lexical))
QN_FUSION_DECLARE_FUNCTIONS(QnPtzMapperPtr, (json))


