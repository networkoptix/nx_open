#include "cloud_db_service_public.h"

#include "cloud_db_service.h"

namespace nx {
namespace cdb {

CloudDbServicePublic::CloudDbServicePublic(int argc, char **argv):
    m_impl(new CloudDbService(argc, argv))
{
}

CloudDbServicePublic::~CloudDbServicePublic()
{
    delete m_impl;
    m_impl = nullptr;
}

void CloudDbServicePublic::pleaseStop()
{
    m_impl->pleaseStop();
}

void CloudDbServicePublic::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    m_impl->setOnStartedEventHandler(std::move(handler));
}

int CloudDbServicePublic::exec()
{
    return m_impl->exec();
}

const CloudDbService* CloudDbServicePublic::impl() const
{
    return m_impl;
}

} // namespace cdb
} // namespace nx
