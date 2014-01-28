/**********************************************************
* 27 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef SYNC_HANDLER_H
#define SYNC_HANDLER_H

#include <condition_variable>
#include <mutex>

#include <QtCore/QObject>

#include "ec_api_impl.h"


namespace ec2
{
    //!Allows executing ec api methods synchronously
    class EC2_LIB_API SyncHandler
    :
        public QObject
    {
        Q_OBJECT

    public:
        SyncHandler();

        void wait();
        ErrorCode errorCode() const;

    public slots:
        void done( ErrorCode errorCode );

    private:
        std::condition_variable m_cond;
        mutable std::mutex m_mutex;
        bool m_done;
        ErrorCode m_errorCode;
    };
}

#endif  //SYNC_HANDLER_H
