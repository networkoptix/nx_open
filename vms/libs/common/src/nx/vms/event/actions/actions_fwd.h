#pragma once

#include <QtCore/QSharedPointer>

namespace nx {
namespace vms {
namespace event {

class CommonAction;
using CommonActionPtr = QSharedPointer<class CommonAction>;

class BookmarkAction;
using BookmarkActionPtr = QSharedPointer<class BookmarkAction>;

class CameraOutputAction;
using CameraOutputActionPtr = QSharedPointer<class CameraOutputAction>;

class PanicAction;
using PanicActionPtr = QSharedPointer<class PanicAction>;

class RecordingAction;
using RecordingActionPtr = QSharedPointer<class RecordingAction>;

class SendMailAction;
using SendMailActionPtr = QSharedPointer<class SendMailAction>;

class SystemHealthAction;
using SystemHealthActionPtr = QSharedPointer<class SystemHealthAction>;

} // namespace event
} // namespace vms
} // namespace nx
