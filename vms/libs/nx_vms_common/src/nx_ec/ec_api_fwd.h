// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

namespace ec2 {

enum class ErrorCode;
enum class NotificationSource;

class AbstractResourceManager;
using AbstractResourceManagerPtr = std::shared_ptr<AbstractResourceManager>;

class AbstractMediaServerManager;
using AbstractMediaServerManagerPtr = std::shared_ptr<AbstractMediaServerManager>;

class AbstractCameraManager;
using AbstractCameraManagerPtr = std::shared_ptr<AbstractCameraManager>;

class AbstractLicenseManager;
using AbstractLicenseManagerPtr = std::shared_ptr<AbstractLicenseManager>;

class AbstractEventRulesManager;
using AbstractEventRulesManagerPtr = std::shared_ptr<AbstractEventRulesManager>;

class AbstractVmsRulesManager;
using AbstractVmsRulesManagerPtr = std::shared_ptr<AbstractVmsRulesManager>;

class AbstractUserManager;
using AbstractUserManagerPtr = std::shared_ptr<AbstractUserManager>;

class AbstractLayoutManager;
using AbstractLayoutManagerPtr = std::shared_ptr<AbstractLayoutManager>;

class AbstractLayoutTourManager;
using AbstractLayoutTourManagerPtr = std::shared_ptr<AbstractLayoutTourManager>;

class AbstractVideowallManager;
using AbstractVideowallManagerPtr = std::shared_ptr<AbstractVideowallManager>;

class AbstractStoredFileManager;
using AbstractStoredFileManagerPtr = std::shared_ptr<AbstractStoredFileManager>;

class AbstractMiscManager;
using AbstractMiscManagerPtr = std::shared_ptr<AbstractMiscManager>;

class AbstractDiscoveryManager;
using AbstractDiscoveryManagerPtr = std::shared_ptr<AbstractDiscoveryManager>;

class AbstractWebPageManager;
using AbstractWebPageManagerPtr = std::shared_ptr<AbstractWebPageManager>;

class AbstractAnalyticsManager;
using AbstractAnalyticsManagerPtr = std::shared_ptr<AbstractAnalyticsManager>;


class AbstractResourceNotificationManager;
using AbstractResourceNotificationManagerPtr = std::shared_ptr<AbstractResourceNotificationManager>;

class AbstractMediaServerNotificationManager;
using AbstractMediaServerNotificationManagerPtr = std::shared_ptr<AbstractMediaServerNotificationManager>;

class AbstractCameraNotificationManager;
using AbstractCameraNotificationManagerPtr = std::shared_ptr<AbstractCameraNotificationManager>;

class AbstractLayoutNotificationManager;
using AbstractLayoutNotificationManagerPtr = std::shared_ptr<AbstractLayoutNotificationManager>;

class AbstractLayoutTourNotificationManager;
using AbstractLayoutTourNotificationManagerPtr = std::shared_ptr<AbstractLayoutTourNotificationManager>;

class AbstractVideowallNotificationManager;
using AbstractVideowallNotificationManagerPtr = std::shared_ptr<AbstractVideowallNotificationManager>;

class AbstractStoredFileNotificationManager;
using AbstractStoredFileNotificationManagerPtr = std::shared_ptr<AbstractStoredFileNotificationManager>;

class AbstractMiscNotificationManager;
using AbstractMiscNotificationManagerPtr = std::shared_ptr<AbstractMiscNotificationManager>;

class AbstractDiscoveryNotificationManager;
using AbstractDiscoveryNotificationManagerPtr = std::shared_ptr<AbstractDiscoveryNotificationManager>;

class AbstractWebPageNotificationManager;
using AbstractWebPageNotificationManagerPtr = std::shared_ptr<AbstractWebPageNotificationManager>;

class AbstractAnalyticsNotificationManager;
using AbstractAnalyticsNotificationManagerPtr = std::shared_ptr<AbstractAnalyticsNotificationManager>;

class AbstractLicenseNotificationManager;
using AbstractLicenseNotificationManagerPtr = std::shared_ptr<AbstractLicenseNotificationManager>;

class AbstractTimeNotificationManager;
using AbstractTimeNotificationManagerPtr = std::shared_ptr<AbstractTimeNotificationManager>;

class AbstractBusinessEventNotificationManager;
using AbstractBusinessEventNotificationManagerPtr = std::shared_ptr<AbstractBusinessEventNotificationManager>;

class AbstractVmsRulesNotificationManager;
using AbstractVmsRulesNotificationManagerPtr = std::shared_ptr<AbstractVmsRulesNotificationManager>;

class AbstractUserNotificationManager;
using AbstractUserNotificationManagerPtr = std::shared_ptr<AbstractUserNotificationManager>;


class AbstractECConnection;
using AbstractECConnectionPtr = std::shared_ptr<AbstractECConnection>;

class AbstractECConnectionFactory;

} // namespace ec2
