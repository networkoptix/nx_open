#pragma once

namespace nx{
namespace plugins{
namespace flir{
namespace nexus{

const QString kAlarmPrefix = lit("$ALARM");
const QString kThgSpotPrefix = lit("$THGSPOT");
const QString kThgAreaPrefix = lit("$THGAREA");
const QString kDigitalInputPrefix = lit("$DI");
const QString kDigitalOutputPrefix = lit("$DO");

const int kMdDeviceType = 12;
const int kIODeviceType = 28;
const int kThgObjectDeviceType = 5;
const int kThgSpotDeviceType = 53;
const int kThgAreaDeviceType = 54;

const QString kCommandPrefix = lit("Nexus.cgi");
const QString kControlPrefix = lit("NexusWS.cgi");
const QString kSubscriptionPrefix = lit("NexusWS_Status.cgi");
const QString kServerWhoAmICommand = lit("SERVERWhoAmI");
const QString kRequestControlCommand = lit("SERVERRemoteControlRequest");
const QString kReleaseControlCommand = lit("SERVERRemoteControlRelease");
const QString kSetOutputPortStateCommand = lit("IOSENSOROutputStateSet");

const QString kSessionParamName = lit("session");
const QString kSubscriptionNumParamName = lit("numSubscriptions");
const QString kNotificationFormatParamName = lit("NotificationFormat");

const QString kJsonNotificationFormat = lit("JSON");
const QString kStringNotificationFormat = lit("String");

const int kNoError = 0;
const int kAnyDevice = -1;

const QString kAlarmSubscription = lit("ALARM");
const QString kThgSpotSubscription = lit("THGSPOT");
const QString kThgAreaSubscription = lit("THGAREA");
const QString kIOSubscription = lit("IO");

const QString kDateTimeFormat("yyyyMMddhhmmsszzz");

const QString kSessionIdParamName = lit("Id");

} //namespace nexus
} //namespace flir
} //namespace plugins
} //namespace nx