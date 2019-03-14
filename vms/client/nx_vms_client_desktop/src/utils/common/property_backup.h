#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QByteArray>

#include "scoped_value_rollback.h"

/*
 * QnAbstractPropertyBackup
 */
class QnAbstractPropertyBackup : public QObject
{
    typedef QObject base_type;

public:
    virtual ~QnAbstractPropertyBackup();

 /* static void backup(object, backupId) should be implemented in subclasses. */
    static bool restore(QObject* object, const char* backupId);

protected:
    QnAbstractPropertyBackup(QObject* parent, const char* backupId);
    QnAbstractPropertyBackup(const QnAbstractPropertyBackup&) = delete;

private:
    QObject* m_object;
    QByteArray m_backupObjectPropertyName;
    QPointer<QObject> m_previousBackup;
};

/*
 * QnPropertyBackup
 */
class QnPropertyBackup : public QnAbstractPropertyBackup
{
    typedef QnAbstractPropertyBackup base_type;

public:
    static void backup(QObject* object, const char* propertyName);
    static void backup(QObject* object, const char* propertyName, const char* backupId);

private:
    QnPropertyBackup(QObject* parent, const char* propertyName, const char* backupId);

private:
    QnScopedPropertyRollback m_propertyRollback;
};

/*
 * QnTypedPropertyBackup
 */
template<class T, class Object>
class QnTypedPropertyBackup : public QnAbstractPropertyBackup
{
    typedef QnAbstractPropertyBackup base_type;

public:
    template<class Get, class Set>
    static void backup(Object* object, Get get, Set set, const char* backupId)
    {
        new QnTypedPropertyBackup(object, get, set, backupId);
    }

private:
    template<class Get, class Set>
    QnTypedPropertyBackup(Object* parent, Get get, Set set, const char* backupId) :
        base_type(parent, backupId),
        m_propertyRollback(parent, set, get)
    {
        connect(parent, &QObject::destroyed, this, [this]() { m_propertyRollback.commit(); });
    }

private:
    QnScopedTypedPropertyRollback<T, Object> m_propertyRollback;
};
