/**********************************************************
* Nov 25, 2015
* akolesnikov
***********************************************************/

#include "cloud_db_process_public.h"

#include "cloud_db_process.h"


namespace nx {
namespace cdb {

CloudDBProcessPublic::CloudDBProcessPublic(int argc, char **argv)
:
    m_impl(new CloudDBProcess(argc, argv))
{
}

CloudDBProcessPublic::~CloudDBProcessPublic()
{
    delete m_impl;
    m_impl = nullptr;
}

void CloudDBProcessPublic::pleaseStop()
{
    m_impl->pleaseStop();
}

void CloudDBProcessPublic::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    m_impl->setOnStartedEventHandler(std::move(handler));
}

int CloudDBProcessPublic::exec()
{
    return m_impl->exec();
}

const CloudDBProcess* CloudDBProcessPublic::impl() const
{
    return m_impl;
}

}   //cdb
}   //nx
