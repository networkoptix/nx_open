#ifndef QN_EC_API_FWD_H
#define QN_EC_API_FWD_H

#include <memory> /* For std::shared_ptr. */

namespace ec2
{
    enum class ErrorCode;
    struct QnFullResourceData; // TODO: #Elric move these out?
    struct QnPeerTimeInfo;
    typedef QList<QnPeerTimeInfo> QnPeerTimeInfoList;

    class AbstractResourceManager;
    typedef std::shared_ptr<AbstractResourceManager> AbstractResourceManagerPtr;

    class AbstractMediaServerManager;
    typedef std::shared_ptr<AbstractMediaServerManager> AbstractMediaServerManagerPtr;

    class AbstractCameraManager;
    typedef std::shared_ptr<AbstractCameraManager> AbstractCameraManagerPtr;

    class AbstractLicenseManager;
    typedef std::shared_ptr<AbstractLicenseManager> AbstractLicenseManagerPtr;

    class AbstractBusinessEventManager;
    typedef std::shared_ptr<AbstractBusinessEventManager> AbstractBusinessEventManagerPtr;

    class AbstractUserManager;
    typedef std::shared_ptr<AbstractUserManager> AbstractUserManagerPtr;

    class AbstractLayoutManager;
    typedef std::shared_ptr<AbstractLayoutManager> AbstractLayoutManagerPtr;

    class AbstractVideowallManager;
    typedef std::shared_ptr<AbstractVideowallManager> AbstractVideowallManagerPtr;

    class AbstractStoredFileManager;
    typedef std::shared_ptr<AbstractStoredFileManager> AbstractStoredFileManagerPtr;

    class AbstractUpdatesManager;
    typedef std::shared_ptr<AbstractUpdatesManager> AbstractUpdatesManagerPtr;

    class AbstractECConnection;
    typedef std::shared_ptr<AbstractECConnection> AbstractECConnectionPtr;

    class AbstractECConnectionFactory;
}

#endif // QN_EC_API_FWD_H
