// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <chrono>

class QObject;
class QThread;
class QTimer;

enum { kDefaultDelay = 1 };

typedef std::function<void ()> Callback;

/// @brief Executes specified functor in the specified <targetThread>
/// after <delay> has expired
NX_VMS_COMMON_API void executeDelayed(
    Callback callback,
    int delayMs = kDefaultDelay,
    QThread* targetThread = nullptr);

/// @brief Executes specified functor in the same thread using timer
/// parented to specified <parent> after <delay> has expired
/// @return Timer could be used for delayed action cancellation.
/// Note: if delayed action is already executed, timer is invalid
/// You have to delete timer to prevent callback to be executed
NX_VMS_COMMON_API QTimer* executeDelayedParented(Callback callback, int delayMs, QObject* parent);

/// @brief Executes specified functor in the same thread using timer
/// parented to specified <parent> after <delay> has expired
/// @return Timer could be used for delayed action cancellation.
/// Note: if delayed action is already executed, timer is invalid
/// You have to delete timer to prevent callback to be executed
NX_VMS_COMMON_API QTimer* executeDelayedParented(
    Callback callback,
    std::chrono::milliseconds delay,
    QObject* parent);

/**
 * Execute specified functor in the thread of the provided parent. If parent is destroyed, callback
 * will not be called.
 */
NX_VMS_COMMON_API QTimer* executeDelayedParented(Callback callback, QObject* parent);

NX_VMS_COMMON_API void executeLater(Callback callback, QObject* parent = nullptr);

NX_VMS_COMMON_API void executeLaterInThread(Callback callback, QThread* thread);

NX_VMS_COMMON_API void executeInThread(QThread* thread, Callback callback);
