#include "desktop_data_provider_wrapper.h"

#if !defined(Q_OS_WIN)

QnDesktopDataProviderWrapper::QnDesktopDataProviderWrapper(
    QnResourcePtr res,
    QnDesktopDataProvider* /*owner*/)
    :
    QnAbstractMediaStreamDataProvider(res),
    QnAbstractDataConsumer(100)
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

#endif // !defined(Q_OS_WIN)
