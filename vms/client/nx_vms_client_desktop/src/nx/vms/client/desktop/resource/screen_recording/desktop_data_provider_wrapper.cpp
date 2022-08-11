// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_data_provider_wrapper.h"

#if !defined(Q_OS_WIN)

namespace nx::vms::client::desktop {

DesktopDataProviderWrapper::DesktopDataProviderWrapper(
    QnResourcePtr res,
    core::DesktopDataProviderBase* owner)
    :
    QnAbstractMediaStreamDataProvider(res),
    m_owner(owner)
{}

DesktopDataProviderWrapper::~DesktopDataProviderWrapper()
{}

void DesktopDataProviderWrapper::putData(const QnAbstractDataPacketPtr& /*data*/)
{}

void DesktopDataProviderWrapper::start(Priority /*priority*/)
{}

bool DesktopDataProviderWrapper::isInitialized() const
{
    return false;
}

QString DesktopDataProviderWrapper::lastErrorStr() const
{
    return QString();
}

bool DesktopDataProviderWrapper::needConfigureProvider() const
{
    return false;
}

bool DesktopDataProviderWrapper::canAcceptData() const
{
    return true;
}

} // namespace nx::vms::client::desktop

#endif // !defined(Q_OS_WIN)
