// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <Foundation/Foundation.h>

namespace extension {

class Logger
{
public:
    /* Log message to the system stream an try to upload to the cloud. */
    static void log(const NSString* message);

    /* Log message to the system stream an try to upload to the cloud. */
    static void log(
        const NSString* logTag,
        const NSString* message);

    /* Log message to the system stream only. */
    static void logToSystemOnly(const NSString* message);

    /* Log message to the system stream only. */
    static void logToSystemOnly(
        const NSString* logTag,
        const NSString* message);

private:
    Logger();

    ~Logger();

    static Logger& instance();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace extension
