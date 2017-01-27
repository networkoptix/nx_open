#include "show_once.h"

#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>

namespace {

QString pathForId(const QString& id)
{
    QDir target(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
    return target.absoluteFilePath(id);
}

}

namespace nx {
namespace settings {

ShowOnce::ShowOnce(const QString& id, QObject* parent):
    base_type(parent),
    m_storage(new QSettings(pathForId(id), QSettings::IniFormat))
{
    m_storage->sync();
}

ShowOnce::~ShowOnce()
{
    m_storage->sync();
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

} // namespace settings
} // namespace nx

