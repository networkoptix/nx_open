#include "keychain_property_storage_backend.h"

#include <QtCore/QEventLoop>

#include <keychain.h>

#include <nx/utils/log/log.h>

namespace nx::client::core {

KeychainBackend::KeychainBackend(const QString& serviceName):
    m_serviceName(serviceName)
{
}

QString KeychainBackend::readValue(const QString& name)
{
    QKeychain::ReadPasswordJob job(m_serviceName);
    job.setAutoDelete(false);
    job.setKey(name);

    QEventLoop eventLoop;

    QObject::connect(&job, &QKeychain::Job::finished, &eventLoop, &QEventLoop::quit,
        Qt::QueuedConnection);

    job.start();
    eventLoop.exec();

    if (job.error() != QKeychain::NoError)
    {
        NX_WARNING(this) << lm("Error while reading value from keychain: %1").arg(job.errorString());
        return {};
    }

    return job.textData();
}

void KeychainBackend::writeValue(const QString& name, const QString& value)
{
    QKeychain::WritePasswordJob job(m_serviceName);
    job.setAutoDelete(false);
    job.setKey(name);
    job.setTextData(value);

    QEventLoop eventLoop;

    QObject::connect(&job, &QKeychain::Job::finished, &eventLoop, &QEventLoop::quit,
        Qt::QueuedConnection);

    job.start();
    eventLoop.exec();

    if (job.error() != QKeychain::NoError)
    {
        NX_WARNING(this) << lm("Error while writing value to keychain: %1").arg(job.errorString());
    }
}

void KeychainBackend::removeValue(const QString& name)
{
    QKeychain::DeletePasswordJob job(m_serviceName);
    job.setAutoDelete(false);
    job.setKey(name);

    QEventLoop eventLoop;

    QObject::connect(&job, &QKeychain::Job::finished, &eventLoop, &QEventLoop::quit,
        Qt::QueuedConnection);

    job.start();
    eventLoop.exec();

    if (job.error() != QKeychain::NoError)
    {
        NX_WARNING(this)
            << lm("Error while deleting value from keychain: %1").arg(job.errorString());
    }
}

} // namespace nx::client::core
