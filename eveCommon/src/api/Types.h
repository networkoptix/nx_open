#ifndef _EVE_TYPES_H
#define _EVE_TYPES_H

#include "xsd_resourceTypes.h"
#include "xsd_resources.h"
#include "xsd_resourcesEx.h"
#include "xsd_layouts.h"
#include "xsd_cameras.h"
#include "xsd_servers.h"
#include "xsd_users.h"
#include "xsd_events.h"
#include "xsd_RecordedTimePeriods.h"

// for appServer methods

typedef xsd::cxx::tree::sequence<xsd::api::resourceTypes::ResourceType>  QnApiResourceTypes;
typedef xsd::cxx::tree::sequence<xsd::api::resources::Resource>          QnApiResources;

typedef xsd::cxx::tree::sequence<xsd::api::cameras::Camera>              QnApiCameras;
typedef xsd::cxx::tree::sequence<xsd::api::layouts::Layout>              QnApiLayouts;
typedef xsd::cxx::tree::sequence<xsd::api::users::User>                  QnApiUsers;
typedef xsd::cxx::tree::sequence<xsd::api::servers::Server>              QnApiServers;

typedef QSharedPointer<xsd::api::resourceTypes::ResourceTypes>           QnApiResourceTypeResponsePtr;
typedef QSharedPointer<xsd::api::resourcesEx::Resources>                 QnApiResourceResponsePtr;

typedef QSharedPointer<xsd::api::cameras::Cameras>                       QnApiCameraResponsePtr;
typedef QSharedPointer<xsd::api::layouts::Layouts>                       QnApiLayoutResponsePtr;
typedef QSharedPointer<xsd::api::users::Users>                           QnApiUserResponsePtr;
typedef QSharedPointer<xsd::api::servers::Servers>                       QnApiServerResponsePtr;

typedef QSharedPointer<xsd::api::events::events_t>                       QnApiEventResponsePtr;

// for video server methods
typedef QSharedPointer<xsd::api::timePeriodList::TimePeriodList>           QnApiTimePeriodListResponsePtr;


#endif // _EVE_TYPES_H
