// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

namespace nx::vms::event {

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

class IntercomCallAction;
using IntercomCallActionPtr = QSharedPointer<class IntercomCallAction>;

} // namespace nx::vms::event
