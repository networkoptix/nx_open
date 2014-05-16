#include "dummy_handler.h"

namespace ec2
{
    static DummyHandler* DummyHandler_instance = nullptr;

    DummyHandler::DummyHandler()
    {
        DummyHandler_instance = this;
    }

    DummyHandler::~DummyHandler()
    {
        DummyHandler_instance = nullptr;
    }

    DummyHandler* DummyHandler::instance()
    {
        return DummyHandler_instance;
    }

    void DummyHandler::onRequestDone( int /*reqID*/, ec2::ErrorCode /*errorCode*/ )
    {
        //TODO/IMPL
    }
}
