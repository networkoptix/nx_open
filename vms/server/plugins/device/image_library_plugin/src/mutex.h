// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class MutexImpl;

//!Non-recursive non-named mutex
/*!
    Implementing our own mutex so that no to use any third party library
*/
class Mutex
{
public:
    class ScopedLock
    {
    public:
        ScopedLock( Mutex* mtx )
        :
            m_mtx( mtx )
        {
            m_mtx->lock();
        }
        ~ScopedLock()
        {
            m_mtx->unlock();
        }

    private:
        Mutex* m_mtx;
    };

    Mutex();
    ~Mutex();

private:
    MutexImpl* m_impl;

    void lock();
    void unlock();
};
