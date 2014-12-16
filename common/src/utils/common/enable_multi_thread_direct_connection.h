/**********************************************************
* 16 dec 2014
* a.kolesnikov
***********************************************************/

#ifndef ENABLE_MULTI_THREAD_DIRECT_CONNECTION_H
#define ENABLE_MULTI_THREAD_DIRECT_CONNECTION_H

#include <QtCore/QMutex>
#include <QtCore/QObject>


//!QObject's successors which allow using Qt::DirectConnection to connect object living in different thread should inherit this class
class EnableMultiThreadDirectConnection
{
public:
    /*!
        \param pObject pointer to object emitting signals. Typically, pointer to class inheriting this one and QObject
    */
    EnableMultiThreadDirectConnection( QObject* pObject );
    virtual ~EnableMultiThreadDirectConnection();

    //!Disconnects \a receiver from all signals of this class and waits for issued \a emit calls connected to \a receiver to return
    /*!
        Can be called from any thread
    */
    void disconnectAndJoin( QObject* receiver );

protected:
    //!Successors have to lock this mutex for emiting signal which allows to be connected to directly
    QMutex m_signalEmitMutex;

private:
    QObject* m_pObject;
};

#endif  //ENABLE_MULTI_THREAD_DIRECT_CONNECTION_H
