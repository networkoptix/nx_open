#ifndef DUMMY_HANDLER_H
#define DUMMY_HANDLER_H

#include <QObject>

#include <nx_ec/ec_api.h>


namespace ec2
{
    class DummyHandler
    :
        public QObject
    {
        Q_OBJECT

    public:
        DummyHandler();
        ~DummyHandler();

        static DummyHandler* instance();

    public slots:
        void onRequestDone( int reqID, ec2::ErrorCode errorCode );
    };
}

#endif  //DUMMY_HANDLER_H
