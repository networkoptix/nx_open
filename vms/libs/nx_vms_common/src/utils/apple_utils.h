// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

/**
 * Wraps func() in an @autoreleasepool and calls it.
 *
 * On Apple platforms, background threads (including Qt worker threads) have no run loop and
 * therefore no autorelease pool. Obj-C objects created during execution - e.g. CFString instances
 * produced by NSLog calls - accumulate indefinitely until the thread exits, leading to unbounded
 * memory growth during long-running operations such as continuous video playback.
 *
 * Call this instead of invoking func() directly to ensure the pool is drained on each call.
 */
void drainAutoreleasePoolAndCall(const std::function<void()>& func);
