#ifndef _EVE_TYPES_H
#define _EVE_TYPES_H

#include <QSharedPointer>

#include "xsd_resourceTypes.h"
#include "xsd_resources.h"
#include "xsd_resourcesEx.h"
#include "xsd_layouts.h"
#include "xsd_cameras.h"
#include "xsd_servers.h"
#include "xsd_users.h"
#include "xsd_events.h"
#include "xsd_storages.h"
#include "xsd_scheduleTasks.h"
#include "xsd_RecordedTimePeriods.h"

// for appServer methods

typedef xsd::cxx::tree::sequence<xsd::api::resourceTypes::ResourceType>  QnApiResourceTypes;
typedef xsd::cxx::tree::sequence<xsd::api::resources::Resource>          QnApiResources;

typedef xsd::cxx::tree::sequence<xsd::api::cameras::Camera>              QnApiCameras;
typedef xsd::cxx::tree::sequence<xsd::api::layouts::Layout>              QnApiLayouts;
typedef xsd::cxx::tree::sequence<xsd::api::users::User>                  QnApiUsers;
typedef xsd::cxx::tree::sequence<xsd::api::servers::Server>              QnApiServers;
typedef xsd::cxx::tree::sequence<xsd::api::storages::Storage>            QnApiStorages;
typedef xsd::cxx::tree::sequence<xsd::api::scheduleTasks::ScheduleTask>  QnApiScheduleTasks;

typedef QSharedPointer<xsd::api::resourceTypes::ResourceTypes>           QnApiResourceTypeResponsePtr;
typedef QSharedPointer<xsd::api::resourcesEx::Resources>                 QnApiResourceResponsePtr;

typedef QSharedPointer<xsd::api::cameras::Cameras>                       QnApiCameraResponsePtr;
typedef QSharedPointer<xsd::api::layouts::Layouts>                       QnApiLayoutResponsePtr;
typedef QSharedPointer<xsd::api::users::Users>                           QnApiUserResponsePtr;
typedef QSharedPointer<xsd::api::servers::Servers>                       QnApiServerResponsePtr;
typedef QSharedPointer<xsd::api::storages::Storages>                     QnApiStorageResponsePtr;
typedef QSharedPointer<xsd::api::scheduleTasks::ScheduleTasks>           QnApiScheduleTaskResponsePtr;

typedef QSharedPointer<xsd::api::events::events_t>                       QnApiEventResponsePtr;

// for video server methods
typedef QSharedPointer<xsd::api::recordedTimePeriods::RecordedTimePeriods>           QnApiRecordedTimePeriodsResponsePtr;


#endif // _EVE_TYPES_H
