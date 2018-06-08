#pragma once

#include <memory> /* For std::shared_ptr. */

#include <QtCore/QList>

namespace ec2 {

enum class ErrorCode;
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

class AbstractEventRulesManager;
using AbstractEventRulesManagerPtr = std::shared_ptr<AbstractEventRulesManager>;

class AbstractUserManager;
typedef std::shared_ptr<AbstractUserManager> AbstractUserManagerPtr;

class AbstractLayoutManager;
typedef std::shared_ptr<AbstractLayoutManager> AbstractLayoutManagerPtr;

class AbstractLayoutTourManager;
using AbstractLayoutTourManagerPtr = std::shared_ptr<AbstractLayoutTourManager>;

class AbstractVideowallManager;
typedef std::shared_ptr<AbstractVideowallManager> AbstractVideowallManagerPtr;

class AbstractWebPageManager;
typedef std::shared_ptr<AbstractWebPageManager> AbstractWebPageManagerPtr;

class AbstractStoredFileManager;
typedef std::shared_ptr<AbstractStoredFileManager> AbstractStoredFileManagerPtr;

class AbstractUpdatesManager;
typedef std::shared_ptr<AbstractUpdatesManager> AbstractUpdatesManagerPtr;

class AbstractECConnection;
typedef std::shared_ptr<AbstractECConnection> AbstractECConnectionPtr;

class AbstractECConnectionFactory;
class RemoteConnectionFactory;


} // namespace ec2
