// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QThread;

/**
 * \param thread                        Thread to check.
 * \returns                             Whether the given thread has a running event loop.
 *
 * \note                                This function always returns true if Qt
 *                                      private headers were not available during
 *                                      compilation. Therefore it should be used for
 *                                      error checking only.
 */
NX_VMS_COMMON_API bool qnHasEventLoop(QThread* thread);
