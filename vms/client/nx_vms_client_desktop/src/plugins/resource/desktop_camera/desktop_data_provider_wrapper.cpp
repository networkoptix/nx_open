// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_data_provider_wrapper.h"

#if !defined(Q_OS_WIN)

QnDesktopDataProviderWrapper::QnDesktopDataProviderWrapper(
    QnResourcePtr res,
    QnDesktopDataProviderBase* owner)
    :
    QnAbstractMediaStreamDataProvider(res),
    m_owner(owner)
{}

QnDesktopDataProviderWrapper::~QnDesktopDataProviderWrapper()
{}

void QnDesktopDataProviderWrapper::putData(const QnAbstractDataPacketPtr& /*data*/)
{}

void QnDesktopDataProviderWrapper::start(Priority /*priority*/)
{}

bool QnDesktopDataProviderWrapper::isInitialized() const
{
    return false;
}

QString QnDesktopDataProviderWrapper::lastErrorStr() const
{
    return QString();
}

bool QnDesktopDataProviderWrapper::needConfigureProvider() const
{
    return false;
}

bool QnDesktopDataProviderWrapper::canAcceptData() const
{
    return true;
}

#endif // !defined(Q_OS_WIN)
