#include "property_storage.h"

#include <QtCore/QMetaType>
#include <QtCore/QSettings>

#include "warnings.h"

QnPropertyStorage::QnPropertyStorage(QObject *parent):
    QObject(parent)
{
}

QnPropertyStorage::~QnPropertyStorage() {
    return;
}

QVariant QnPropertyStorage::value(int id) const {
    lock();
    QVariant result = m_valueById.value(id);
    unlock();
    return result;
}

bool QnPropertyStorage::setValue(int id, const QVariant &value) {
    QVariant newValue = value;

    int type = this->type(id);
    if(type != QMetaType::Void && value.type() != type) {
        if(!newValue.convert(static_cast<QVariant::Type>(type))) {
            qnWarning("Cannot assign a value of type '%1' to a property '%2' of type '%3'.", QMetaType::typeName(value.userType()), name(id), QMetaType::typeName(type));
            return false;
        }
    }

    lock();
    if(m_valueById.value(id) == newValue) {
        unlock();
        return false;
    }

    m_valueById[id] = value;

    QnPropertyNotifier *notifier = m_notifiers.value(id);
    unlock();

    emit valueChanged(id);
    if(notifier)
        emit notifier->valueChanged(id);

    return true;
}

QnPropertyNotifier *QnPropertyStorage::notifier(int id) const {
    lock();
    QnPropertyNotifier *&result = m_notifiers[id];
    if(!result)
        result = new QnPropertyNotifier(const_cast<QnPropertyStorage *>(this));
    unlock();
    
    return result;
}

QString QnPropertyStorage::name(int id) const {
    return m_nameById.value(id);
}

void QnPropertyStorage::setName(int id, const QString &name) {
    m_nameById[id] = name;
}

int QnPropertyStorage::type(int id) const {
    return m_typeById.value(id, QMetaType::Void);
}

void QnPropertyStorage::setType(int id, int type) {
    if(m_typeById[id] == type)
        return;

    m_typeById[id] = type;

    if(type != QMetaType::Void)
        setValue(id, value(id)); /* Re-convert. */
}

void QnPropertyStorage::updateFromSettings(QSettings *settings) {
    if(!settings) {
        qnNullWarning(settings);
        return;
    }

    lock();
    foreach(int id, m_nameById.keys())
        setValue(id, updateValueFromSettings(settings, id, value(id)));
    unlock();
}

void QnPropertyStorage::submitToSettings(QSettings *settings) const {
    if(!settings) {
        qnNullWarning(settings);
        return;
    }

    lock();
    foreach(int id, m_nameById.keys())
        submitValueToSettings(settings, id, value(id));
    unlock();
}

QVariant QnPropertyStorage::updateValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue) {
    return settings->value(name(id), defaultValue);
}

void QnPropertyStorage::submitValueToSettings(QSettings *settings, int id, const QVariant &value) const {
    settings->setValue(name(id), value);
}

