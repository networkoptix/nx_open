// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "keychain_property_storage_backend.h"

#include <QtCore/QEventLoop>

#include <keychain.h>

#include <nx/build_info.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

KeychainBackend::KeychainBackend(const QString& serviceName):
    m_serviceName(serviceName)
{
    NX_INFO(this, "Created. Service name: \"%1\"", serviceName);
    if (nx::build_info::isLinux())
    {
        // If autologin on Ubuntu is configured, keyring should be initialized manually.
        writeValue("", ""); //< Force user to enter password.
    }

}

QString KeychainBackend::readValue(const QString& name, bool* success)
{
    NX_VERBOSE(this, "Reading \"%1\" from keychain...", name);

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
        NX_WARNING(this, "Error while reading \"%1\" from keychain: %2", job.errorString(), name);
        return {};
    }

    NX_VERBOSE(this, "Successfully read \"%1\" from keychain", name);
    return job.textData();
}

bool KeychainBackend::writeValue(const QString& name, const QString& value)
{
    NX_VERBOSE(this, "Writing \"%1\" to keychain...", name);

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
        NX_WARNING(this, "Error while writing \"%1\" to keychain: %2", name, job.errorString());

    NX_VERBOSE(this, "Successfully wrote \"%1\" to keychain", name);
    return ok;
}

bool KeychainBackend::removeValue(const QString& name)
{
    NX_VERBOSE(this, "Deleting \"%1\" from keychain...", name);

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
        NX_WARNING(this, "Error while deleting \"%1\" from keychain: %2", name, job.errorString());

    NX_VERBOSE(this, "Successfully deleted \"%1\" from keychain", name);
    return ok;
}

} // namespace nx::vms::client::core
