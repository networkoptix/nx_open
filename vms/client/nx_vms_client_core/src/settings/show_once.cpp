// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_once.h"

#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>

#include <nx/utils/log/assert.h>

namespace {

QString pathForId(const QString& id)
{
    QDir target(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    return target.absoluteFilePath(id);
}

} // namespace

namespace nx {
namespace settings {

ShowOnce::ShowOnce(const QString& id, StorageFormat format, QObject* parent):
    base_type(parent)
{
    switch (format)
    {
        case StorageFormat::File:
            m_storage.reset(new QSettings(pathForId(id), QSettings::IniFormat));
            break;
        default:
            NX_ASSERT(format == StorageFormat::Section);
            m_storage.reset(new QSettings());
            m_storage->beginGroup(id);
            break;
    }

    m_storage->sync();
}

ShowOnce::~ShowOnce()
{
}

bool ShowOnce::testFlag(const QString& key) const
{
    return m_storage->contains(key);
}

void ShowOnce::setFlag(const QString& key, bool value)
{
    if (value == testFlag(key))
        return;

    if (value)
        m_storage->setValue(key, value);
    else
        m_storage->remove(key);
    m_storage->sync();

    emit changed(key, value);
}

void ShowOnce::sync()
{
    m_storage->sync();
}

void ShowOnce::reset()
{
    const auto keys = m_storage->allKeys();
    const auto group = m_storage->group();

    // Reset all the values in current group.
    m_storage->endGroup();
    m_storage->remove(group);
    m_storage->beginGroup(group);

    m_storage->sync();

    for (const QString& key: keys)
        emit changed(key, false);
}

} // namespace settings
} // namespace nx

