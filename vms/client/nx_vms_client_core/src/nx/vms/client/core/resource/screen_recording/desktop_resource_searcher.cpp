// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_resource_searcher.h"

#include <client_core/client_core_module.h>

#include "abstract_desktop_resource_searcher_impl.h"

namespace nx::vms::client::core {

namespace {

const QString kManufacturer = "Desktop camera";

} // namespace

DesktopResourceSearcher::DesktopResourceSearcher(
    QnAbstractDesktopResourceSearcherImpl* impl,
    QObject* parent)
    :
    base_type(parent),
    QnAbstractResourceSearcher(qnClientCoreModule->commonModule()),
    m_impl(impl)
{
}

DesktopResourceSearcher::~DesktopResourceSearcher()
{
}

QString DesktopResourceSearcher::manufacturer() const
{
    return kManufacturer;
}

QnResourceList DesktopResourceSearcher::findResources()
{
    return m_impl->findResources();
}

bool DesktopResourceSearcher::isResourceTypeSupported(nx::Uuid /*resourceTypeId*/) const
{
    return false;
}

QnResourcePtr DesktopResourceSearcher::createResource(
    const nx::Uuid& /*resourceTypeId*/,
    const QnResourceParams& /*params*/)
{
    return QnResourcePtr();
}

} // namespace nx::vms::client::core
