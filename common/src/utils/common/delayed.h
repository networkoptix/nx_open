#pragma once

class QThread;

enum { kDefaultDelay = 1 };

typedef std::function<void ()> Callback;

/// @brief Executes specified functor in the specified <targetThread>
/// after <delay> has expired
void executeDelayed(const Callback &callback
    , int delayMs = kDefaultDelay
    , QThread *targetThread = nullptr);

/// @brief Executes specified functor in the same thread using timer 
/// parented to specified <parent> after <delay> has expired
void executeDelayedParented(const Callback &callback
    , int delayMs
    , QObject *parent);
