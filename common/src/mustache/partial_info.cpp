#include "mustache/partial_info.h"

QnPartialInfo::QnPartialInfo (BusinessEventType::Value value) {
    switch (value) {
        case BusinessEventType::Camera_Motion: {
            attrName = lit("camera_motion");
            eventLogoFilename = lit("camera.png");
            break;
        }
        case BusinessEventType::Camera_Input: {
            attrName = lit("camera_input");
            eventLogoFilename = lit("camera.png");
            break;
        }

        case BusinessEventType::Camera_Disconnect: {
            attrName = lit("camera_disconnect");
            eventLogoFilename = lit("camera.png");
            break;
        }

        case BusinessEventType::Storage_Failure: {
            attrName = lit("storage_failure");
            eventLogoFilename = lit("storage.png");
            break;
        }

        case BusinessEventType::Network_Issue: {
            attrName = lit("network_issue");
            eventLogoFilename = lit("server.png");
            break;
        }

        case BusinessEventType::Camera_Ip_Conflict: {
            attrName = lit("camera_ip_conflict");
            eventLogoFilename = lit("camera.png");
            break;
        }

        case BusinessEventType::MediaServer_Failure: {
            attrName = lit("mediaserver_failure");
            eventLogoFilename = lit("server.png");
            break;
        }

        case BusinessEventType::MediaServer_Conflict: {
            attrName = lit("mediaserver_conflict");
            eventLogoFilename = lit("server.png");
            break;
        }

        case BusinessEventType::MediaServer_Started: {
            attrName = lit("mediaserver_started");
            eventLogoFilename = lit("server.png");
            break;
        }
    default:
        break;
    }
}
