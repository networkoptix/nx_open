#include "keychain_property_storage_backend.h"

#include <QtCore/QEventLoop>

#include <keychain.h>

#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

KeychainBackend::KeychainBackend(const QString& serviceName):
    m_serviceName(serviceName)
{
}

QString KeychainBackend::readValue(const QString& name, bool* success)
{
    QKeychain::ReadPasswordJob job(m_serviceName);
    job.setAutoDelete(false);
    job.setKey(name);

    QEventLoop eventLoop;

    QObject::connect(&job, &QKeychain::Job::finished, &eventLoop, &QEventLoop::quit,
        Qt::QueuedConnection);

    job.start();
    eventLoop.exec();

    const bool ok = job.error() == QKeychain::NoError;
    if (success)
        *success = ok;

    if (!ok)
    {
        NX_WARNING(this, "Error while reading value from keychain: %1", job.errorString());
        return {};
    }

    return job.textData();
}

bool KeychainBackend::writeValue(const QString& name, const QString& value)
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

    const bool ok = job.error() == QKeychain::NoError;
    if (!ok)
        NX_WARNING(this, "Error while writing value to keychain: %1", job.errorString());

    return ok;
}

bool KeychainBackend::removeValue(const QString& name)
{
    QKeychain::DeletePasswordJob job(m_serviceName);
    job.setAutoDelete(false);
    job.setKey(name);

    QEventLoop eventLoop;

    QObject::connect(&job, &QKeychain::Job::finished, &eventLoop, &QEventLoop::quit,
        Qt::QueuedConnection);

    job.start();
    eventLoop.exec();

    const bool ok = job.error() == QKeychain::NoError;
    if (!ok)
        NX_WARNING(this, "Error while deleting value from keychain: %1", job.errorString());

    return ok;
}

} // namespace nx::vms::client::core
