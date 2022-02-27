// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**
 * Should be implemented by classes that want to allow user to wait for
 * object has finished its asynchronous tasks.
 */
class NX_UTILS_API QnJoinable
{
public:
    virtual ~QnJoinable() = default;

    /**
     * Blocks till object does what it should before destruction.
     */
    virtual void join() = 0;
};
