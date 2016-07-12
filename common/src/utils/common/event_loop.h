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
bool qnHasEventLoop(QThread* thread);
