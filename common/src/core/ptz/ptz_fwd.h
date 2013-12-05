#ifndef QN_PTZ_FWD_H
#define QN_PTZ_FWD_H

#include <QtCore/QSharedPointer>
#include <QtCore/QList>

#include <utils/common/json_fwd.h>

class QnPtzMapper;
typedef QSharedPointer<QnPtzMapper> QnPtzMapperPtr;

class QnAbstractPtzController;
typedef QSharedPointer<QnAbstractPtzController> QnPtzControllerPtr;

class QnPtzPreset;
typedef QList<QnPtzPreset> QnPtzPresetList;

class QnPtzTourSpot;
typedef QList<QnPtzTourSpot> QnPtzTourSpotList;

class QnPtzTour;

QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnPtzPreset)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnPtzTourSpot)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnPtzTour)

#endif // QN_PTZ_FWD_H

