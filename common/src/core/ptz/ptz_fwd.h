#ifndef QN_PTZ_FWD_H
#define QN_PTZ_FWD_H

#include <QtCore/QSharedPointer>
#include <QtCore/QList>

#include <utils/common/model_functions_fwd.h>

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

struct QnPtzLimits;

struct QnPtzData;

struct QnPtzObject;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnPtzMapperPtr)(QnPtzPreset)(QnPtzTourSpot)(QnPtzTour)(QnPtzLimits)(QnPtzData)(QnPtzObject), (json))

#endif // QN_PTZ_FWD_H

