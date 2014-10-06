/**********************************************************
* 2 oct 2014
* akolesnikov
***********************************************************/

#ifndef CAMERA_USER_ATTRIBUTES_H
#define CAMERA_USER_ATTRIBUTES_H

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>

#include <core/misc/schedule_task.h>
#include <core/ptz/media_dewarping_params.h>

#include "motion_window.h"


//!Contains camera settings usually modified by user
/*!
    E.g., recording schedule, motion type, second stream quality, etc...
*/
class QnCameraUserAttributes
{
public:
    QnUuid cameraID;
    Qn::MotionType motionType;
    QList<QnMotionRegion> motionRegions;
    bool scheduleDisabled;
    bool audioEnabled;
    bool cameraControlDisabled;
    QnScheduleTaskList scheduleTasks;
    Qn::SecondStreamQuality secondaryQuality;
    int minDays;
    int maxDays;
    QnUuid preferedServerId;
    //!User-given name of camera (can be different from resource name). This is name shown to the user
    QString name;
    QnMediaDewarpingParams dewarpingParams;

    QnCameraUserAttributes();

    void assign( const QnCameraUserAttributes& right, QSet<QByteArray>* const modifiedFields );
};

Q_DECLARE_METATYPE(QnCameraUserAttributes)
Q_DECLARE_METATYPE(QnCameraUserAttributesPtr)
Q_DECLARE_METATYPE(QnCameraUserAttributesList)

#endif  //CAMERA_USER_ATTRIBUTES_H
