#include "property_backup.h"

#include <QtCore/QScopedPointer>
#include <nx/utils/log/assert.h>

namespace
{
    static const QByteArray kBackupPropertyPrefix = "_qn_backup_";

    QByteArray backupObjectPropertyName(const char* backupId)
    {
        return kBackupPropertyPrefix + backupId;
    }
};

/*
 * QnAbstractPropertyBackup
 */
QnAbstractPropertyBackup::QnAbstractPropertyBackup(QObject* parent, const char* backupId) :
    base_type(parent),
    m_object(parent),
    m_backupObjectPropertyName(backupObjectPropertyName(backupId)),
    m_previousBackup(m_object->property(m_backupObjectPropertyName).value<QObject*>())
{
    if (m_previousBackup)
    {
        /* Reparent previous backup for LIFO-order backup-restore: */
        m_previousBackup->setParent(this);
    }

    m_object->setProperty(m_backupObjectPropertyName, QVariant::fromValue(this));
}

QnAbstractPropertyBackup::~QnAbstractPropertyBackup()
{
    NX_ASSERT(parent() == m_object);
    if (m_previousBackup)
    {
        m_previousBackup->setParent(m_object);
        m_object->setProperty(m_backupObjectPropertyName, QVariant::fromValue(m_previousBackup));
    }
    else
    {
        m_object->setProperty(m_backupObjectPropertyName, QVariant());
    }
}

bool QnAbstractPropertyBackup::restore(QObject* object, const char* backupId)
{
    QScopedPointer<QObject> backup(object->property(backupObjectPropertyName(backupId)).value<QObject*>());
    return !backup.isNull();
}

/*
* QnPropertyBackup
*/
QnPropertyBackup::QnPropertyBackup(QObject* parent, const char* propertyName, const char* backupId) :
    base_type(parent, backupId),
    m_propertyRollback(parent, propertyName)
{
    connect(parent, &QObject::destroyed, this, [this]() { m_propertyRollback.commit(); });
}

void QnPropertyBackup::backup(QObject* object, const char* propertyName)
{
    new QnPropertyBackup(object, propertyName, propertyName);
}

void QnPropertyBackup::backup(QObject* object, const char* propertyName, const char* backupId)
{
    new QnPropertyBackup(object, propertyName, backupId);
}
