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
            break;
        }

        case BusinessEventType::Camera_Disconnect: {
            attrName = lit("typeCameraDisconnect");
            break;
        }

        case BusinessEventType::Storage_Failure: {
            attrName = lit("typeStorageFailure");
            break;
        }

        case BusinessEventType::Network_Issue: {
            attrName = lit("typeNetworkIssue");
            break;
        }

        case BusinessEventType::Camera_Ip_Conflict: {
            attrName = lit("typeCameraIpConflict");
            break;
        }

        case BusinessEventType::MediaServer_Failure: {
            attrName = lit("typeMediaServerFailure");
            break;
        }

        case BusinessEventType::MediaServer_Conflict: {
            attrName = lit("typeMediaServerConflict");
            break;
        }
    }
}
