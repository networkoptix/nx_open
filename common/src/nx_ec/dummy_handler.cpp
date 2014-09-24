#include "dummy_handler.h"

namespace ec2
{
    DummyHandler* DummyHandler::instance()
    {
        static DummyHandler instance;
        return &instance;
    }

    void DummyHandler::onRequestDone( int /*reqID*/, ec2::ErrorCode /*errorCode*/ )
    {
        //TODO/IMPL
    }
}
