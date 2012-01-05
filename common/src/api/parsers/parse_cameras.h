#ifndef _eve_parse_cameras_
#define _eve_parse_cameras_

#include "api/Types.h"
#include "core/resource/resource.h"
#include "core/resourcemanagment/security_cam_resource.h"
#include "api/parsers/parse_schedule_tasks.h"

// void parseCameras(QnSecurityCamResourceList& cameras, const QnApiCameras& xsdCameras, QnResourceFactory& resourceFactory);

typedef QSharedPointer<xsd::api::cameras::Camera> QnApiCameraPtr;

namespace
{
    void parseRegion(QRegion& region, const QString& regionString)
    {
        foreach (QString rectString, regionString.split(';'))
        {
            QRect rect;
            QStringList rectList = rectString.split(',');

            if (rectList.size() == 4)
            {
                rect.setLeft(rectList[0].toInt());
                rect.setTop(rectList[1].toInt());
                rect.setWidth(rectList[2].toInt());
                rect.setHeight(rectList[3].toInt());
            }

            region += rect;
        }
    }
}


QnApiCameraPtr unparseCamera(const QnCameraResource& cameraIn)
{
    QnApiCameraPtr camera(new xsd::api::cameras::Camera(cameraIn.getId().toString().toStdString(),
                                     cameraIn.getName().toStdString(),
                                     cameraIn.getTypeId().toString().toStdString(),
                                     cameraIn.getUrl().toStdString(),
                                     cameraIn.getMAC().toString().toStdString(),
                                     cameraIn.getAuth().user().toStdString(),
                                     cameraIn.getAuth().password().toStdString()));

    xsd::api::scheduleTasks::ScheduleTasks scheduleTasks;
    foreach(const QnScheduleTask& scheduleTaskIn, cameraIn.getScheduleTasks())
    {
        xsd::api::scheduleTasks::ScheduleTask scheduleTask(
                                                scheduleTaskIn.getStartTime(),
                                                scheduleTaskIn.getEndTime(),
                                                scheduleTaskIn.getDoRecordAudio(),
                                                scheduleTaskIn.getRecordingType(),
                                                scheduleTaskIn.getDayOfWeek(),
                                                scheduleTaskIn.getBeforeThreshold(),
                                                scheduleTaskIn.getAfterThreshold(),
                                                scheduleTaskIn.getStreamQuality(),
                                                scheduleTaskIn.getFps());

        scheduleTasks.scheduleTask().push_back(scheduleTask);
    }

    camera->scheduleTasks(scheduleTasks);
    camera->parentID(cameraIn.getParentId().toString().toStdString());

    return camera;
}

template <class ResourcePtr>
void parseCameras(QList<ResourcePtr>& cameras, const QnApiCameras& xsdCameras, QnResourceFactory& resourceFactory)
{
    using xsd::api::cameras::Cameras;
    using xsd::api::resources::Properties;
    using xsd::api::resourceTypes::ParentIDs;
    using xsd::api::resourceTypes::PropertyTypes;
    using xsd::api::resourceTypes::ParamType;

    for (Cameras::camera_const_iterator i (xsdCameras.begin()); i != xsdCameras.end(); ++i)
    {
        QnResourceParameters parameters;
        parameters["id"] = i->id().c_str();
        parameters["name"] = i->name().c_str();
        parameters["url"] = i->url().c_str();
        parameters["mac"] = i->mac().c_str();
        parameters["login"] = i->login().c_str();
        parameters["password"] = i->password().c_str();

        if (i->status().present())
            parameters["status"] = (*i->status()).c_str();

        if (i->parentID().present())
            parameters["parentId"] = (*i->parentID()).c_str();

        QnResourcePtr cameraBase = resourceFactory.createResource(i->typeId().c_str(), parameters);
        if (cameraBase.isNull())
            continue;

        QnSecurityCamResourcePtr camera = cameraBase.dynamicCast<QnSecurityCamResource>();
        if (camera.isNull())
            continue;

        if (i->region().present())
        {
            QRegion region;
            parseRegion(region, (*i->region()).c_str());
            camera->setMotionMask(region);
        }

        QnParamList& paramList = camera->getResourceParamList();

        if (i->properties().present())
        {
            const Properties& properties = *(i->properties());
            for (Properties::property_const_iterator j(properties.property().begin()); j != properties.property().end(); ++j)
            {
                // TODO add parameter to paramList here
            }
        }

        if (i->scheduleTasks().present())
        {
                QnScheduleTaskList scheduleTaskList;
                parseScheduleTasks(scheduleTaskList, (*(i->scheduleTasks())).scheduleTask(), resourceFactory);
                camera->setScheduleTasks(scheduleTaskList);
        }

        cameras.append(camera);
    }
}

#endif // _eve_parse_cameras_
