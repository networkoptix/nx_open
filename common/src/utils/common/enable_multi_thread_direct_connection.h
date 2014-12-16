/**********************************************************
* 16 dec 2014
* a.kolesnikov
***********************************************************/

#ifndef ENABLE_MULTI_THREAD_DIRECT_CONNECTION_H
#define ENABLE_MULTI_THREAD_DIRECT_CONNECTION_H

#include <QtCore/QMutex>
#include <QtCore/QObject>


//!QObject's successors which allow using Qt::DirectConnection to connect object living in different thread should inherit this class
template<class Derived>
class EnableMultiThreadDirectConnection
{
public:
    EnableMultiThreadDirectConnection()
    :
        m_signalEmitMutex( QMutex::Recursive )
    {
    }

    virtual ~EnableMultiThreadDirectConnection() {}

    //!Disconnects \a receiver from all signals of this class and waits for issued \a emit calls connected to \a receiver to return
    /*!
        Can be called from any thread
    */
    void disconnectAndJoin( QObject* receiver )
    {
        QObject::disconnect( static_cast<Derived*>(this), nullptr, receiver, nullptr );
        QMutexLocker lk( &m_signalEmitMutex );  //waiting for signals to be emitted in other threads to be processed
    }

protected:
    //!Successors have to lock this mutex for emiting signal which allows to be connected to directly
    QMutex m_signalEmitMutex;
};

#endif  //ENABLE_MULTI_THREAD_DIRECT_CONNECTION_H
