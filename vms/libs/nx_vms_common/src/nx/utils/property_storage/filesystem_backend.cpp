// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filesystem_backend.h"

#include <nx/utils/log/log.h>

namespace nx::utils::property_storage {

FileSystemBackend::FileSystemBackend(const QDir& path):
    m_path(path)
{
}

QString FileSystemBackend::readValue(const QString& name, bool* success)
{
    if (success)
        *success = true;

    QFile f(m_path.absoluteFilePath(name));
    if (f.exists())
    {
        if (!f.open(QIODevice::ReadOnly))
        {
            if (success)
                *success = false;
            return {};
        }

        return QString::fromUtf8(f.readAll());
    }

    return {};
}

bool FileSystemBackend::writeValue(const QString& name, const QString& value)
{
    if (!m_path.exists())
    {
        if (!m_path.mkpath(m_path.absolutePath()))
        {
            NX_ERROR(this, "Cannot create folder %1", m_path.absolutePath());
            return false;
        }
    }

    QFile f(m_path.absoluteFilePath(name));
    if (!f.open(QIODevice::WriteOnly))
    {
        NX_ERROR(this, "Cannot create file %1", f.fileName());
        return false;
    }

    f.write(value.toUtf8());
    f.close();
    return true;
}

bool FileSystemBackend::removeValue(const QString& name)
{
    const QString path = m_path.absoluteFilePath(name);
    if (!QFileInfo::exists(path))
        return true;

    return QFile::remove(path);
}

bool FileSystemBackend::sync()
{
    return true;
}

} // namespace nx::utils::property_storage
