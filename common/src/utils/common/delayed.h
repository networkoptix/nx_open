#pragma once

class QThread;
class QTimer;

enum { kDefaultDelay = 1 };

typedef std::function<void ()> Callback;

/// @brief Executes specified functor in the specified <targetThread>
/// after <delay> has expired
void executeDelayed(const Callback &callback
    , int delayMs = kDefaultDelay
    , QThread *targetThread = nullptr);

/// @brief Executes specified functor in the same thread using timer 
/// parented to specified <parent> after <delay> has expired
/// @return Timer could be used for delayed action cancellation. 
/// Note: if delayed action is already executed, timer is invalid
QTimer *executeDelayedParented(const Callback &callback
    , int delayMs
    , QObject *parent);
