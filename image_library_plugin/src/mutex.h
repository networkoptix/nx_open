/**********************************************************
* 10 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef MUTEX_H
#define MUTEX_H


//!Implementing our own mutex so that no to use any third party library
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
    void lock();
    void unlock();
};

#endif  //MUTEX_H
