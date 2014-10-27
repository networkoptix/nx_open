#include "partial_info.h"

#ifdef ENABLE_SENDMAIL

QnPartialInfo::QnPartialInfo (QnBusiness::EventType value) {
    switch (value) {
        case QnBusiness::CameraMotionEvent: {
            attrName = lit("camera_motion");
            eventLogoFilename = lit("camera.png");
            break;
        }
        case QnBusiness::CameraInputEvent: {
            attrName = lit("camera_input");
            eventLogoFilename = lit("camera.png");
            break;
        }

        case QnBusiness::CameraDisconnectEvent: {
            attrName = lit("camera_disconnect");
            eventLogoFilename = lit("camera.png");
            break;
        }

        case QnBusiness::StorageFailureEvent: {
            attrName = lit("storage_failure");
            eventLogoFilename = lit("storage.png");
            break;
        }

        case QnBusiness::NetworkIssueEvent: {
            attrName = lit("network_issue");
            eventLogoFilename = lit("server.png");
            break;
        }

        case QnBusiness::CameraIpConflictEvent: {
            attrName = lit("camera_ip_conflict");
            eventLogoFilename = lit("camera.png");
            break;
        }

        case QnBusiness::ServerFailureEvent: {
            attrName = lit("mediaserver_failure");
            eventLogoFilename = lit("server.png");
            break;
        }

        case QnBusiness::ServerConflictEvent: {
            attrName = lit("mediaserver_conflict");
            eventLogoFilename = lit("server.png");
            break;
        }

        case QnBusiness::ServerStartEvent: {
            attrName = lit("mediaserver_started");
            eventLogoFilename = lit("server.png");
            break;
        }
                                           
        case QnBusiness::LicenseIssueEvent: {
            attrName = lit("license_issue");
            eventLogoFilename = lit("license.png");
            break;
        }
        
    default:
        break;
    }
}

#endif // ENABLE_SENDMAIL
