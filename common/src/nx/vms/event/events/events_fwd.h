#pragma once

#include <QtCore/QSharedPointer>

namespace nx {
namespace vms {
namespace event {

class BackupFinishedEvent;
using BackupFinishedEventPtr = QSharedPointer<class BackupFinishedEvent>;

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

} // namespace event
} // namespace vms
} // namespace nx
