// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

namespace nx {
namespace vms {
namespace event {

class CustomEvent;
using CustomEventPtr = QSharedPointer<class CustomEvent>;

class CameraInputEvent;
using CameraInputEventPtr = QSharedPointer<class CameraInputEvent>;

class CameraDisconnectedEvent;
using CameraDisconnectedEventPtr = QSharedPointer<class CameraDisconnectedEvent>;

class IpConflictEvent;
using IpConflictEventPtr = QSharedPointer<class IpConflictEvent>;

class LicenseIssueEvent;
using LicenseIssueEventPtr = QSharedPointer<class LicenseIssueEvent>;

class MotionEvent;
using MotionEventPtr = QSharedPointer<class MotionEvent>;

class NetworkIssueEvent;
using NetworkIssueEventPtr = QSharedPointer<class NetworkIssueEvent>;

class ServerConflictEvent;
using ServerConflictEventPtr = QSharedPointer<class ServerConflictEvent>;

class ServerFailureEvent;
using ServerFailureEventPtr = QSharedPointer<class ServerFailureEvent>;

class ServerStartedEvent;
using ServerStartedEventPtr = QSharedPointer<class ServerStartedEvent>;

class SoftwareTriggerEvent;
using SoftwareTriggerEventPtr = QSharedPointer<class SoftwareTriggerEvent>;

class StorageFailureEvent;
using StorageFailureEventPtr = QSharedPointer<class StorageFailureEvent>;

class AnalyticsSdkEvent;
using AnalyticsSdkEventPtr = QSharedPointer<class AnalyticsSdkEvent>;

class AnalyticsSdkObjectDetected;
using AnalyticsSdkObjectDetectedPtr = QSharedPointer<class AnalyticsSdkObjectDetected>;

class PluginDiagnosticEvent;
using PluginDiagnosticEventPtr = QSharedPointer<PluginDiagnosticEvent>;

class PoeOverBudgetEvent;
using PoeOverBudgetEventPtr = QSharedPointer<PoeOverBudgetEvent>;

class FanErrorEvent;
using FanErrorEventPtr = QSharedPointer<FanErrorEvent>;

class ServerCertificateError;
using ServerCertificateErrorPtr = QSharedPointer<ServerCertificateError>;

class LdapSyncIssueEvent;
using LdapSyncIssueEventPtr = QSharedPointer<LdapSyncIssueEvent>;

} // namespace event
} // namespace vms
} // namespace nx
