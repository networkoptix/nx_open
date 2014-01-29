#include "mustache/partial_info.h"

QnPartialInfo::QnPartialInfo (BusinessEventType::Value value) {
    switch (value) {
        case BusinessEventType::Camera_Motion: {
            attrName = lit("typeCameraMotion");
            eventLogoFilename = lit("camera.png");
            break;
        }
        case BusinessEventType::Camera_Input: {
            attrName = lit("typeCameraInput");
            eventLogoFilename = lit("camera.png");
            break;
        }

        case BusinessEventType::Camera_Disconnect: {
            attrName = lit("typeCameraDisconnect");
            eventLogoFilename = lit("camera.png");
            break;
        }

        case BusinessEventType::Storage_Failure: {
            attrName = lit("typeStorageFailure");
            eventLogoFilename = lit("storage.png");
            break;
        }

        case BusinessEventType::Network_Issue: {
            attrName = lit("typeNetworkIssue");
            eventLogoFilename = lit("server.png");
            break;
        }

        case BusinessEventType::Camera_Ip_Conflict: {
            attrName = lit("typeCameraIpConflict");
            eventLogoFilename = lit("camera.png");
            break;
        }

        case BusinessEventType::MediaServer_Failure: {
            attrName = lit("typeMediaServerFailure");
            eventLogoFilename = lit("server.png");
            break;
        }

        case BusinessEventType::MediaServer_Conflict: {
            attrName = lit("typeMediaServerConflict");
            eventLogoFilename = lit("server.png");
            break;
        }

        case BusinessEventType::MediaServer_Started: {
            attrName = lit("typeMediaServerStarted");
            eventLogoFilename = lit("server.png");
            break;
        }
    default:
        break;
    }
}
