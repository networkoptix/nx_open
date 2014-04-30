#include "lexical_enum_serializer.h"

#include <cassert>

#include <QtCore/QMetaObject>
#include <QtCore/QMetaEnum>

#include <utils/common/warnings.h>


void QnLexicalEnumSerializerData::insert(int value, const QString &name) {
    if(!m_valueByName.contains(name))
        m_valueByName[name] = value;
    if(!m_nameByValue.contains(value))
        m_nameByValue[value] = name;
}

void QnLexicalEnumSerializerData::insert(const QMetaObject *metaObject, const char *enumName) {
    assert(metaObject && enumName);

    int index = metaObject->indexOfEnumerator(enumName);
    assert(index >= 0); /* Getting an assert here? Do your class actually contain the provided enumeration? */

    QMetaEnum enumerator = metaObject->enumerator(index);
    QString scope = QLatin1String(metaObject->className()) + QStringLiteral("::");
    for(int i = 0; i < enumerator.keyCount(); i++) {
        insert(enumerator.value(i), QLatin1String(enumerator.key(i)));
        insert(enumerator.value(i), scope + QLatin1String(enumerator.key(i))); /* Also support scoped names when deserializing. */
    }

    m_enumName = scope + QLatin1String(enumName);
}

void QnLexicalEnumSerializerData::serializeEnum(int value, QString *target) {
    /* Return empty string in case of failure. */
    *target = m_nameByValue.value(value); 
}

bool QnLexicalEnumSerializerData::deserializeEnum(const QString &value, int *target) {
    auto pos = m_valueByName.find(value);
    if(pos == m_valueByName.end())
        return false;
    
    *target = *pos;
    return true;
}

void QnLexicalEnumSerializerData::serializeFlags(int value, QString *target) {
    target->clear();

    int v = value;
    for(auto pos = m_nameByValue.begin(); pos != m_nameByValue.end(); pos++) {
        int k = pos.key();
        if((k != 0 && (v & k) == k) || k == v) {
            v = v & ~k;

            if(!target->isEmpty())
                *target += L'|';
            *target += pos.value();

            if(v == 0)
                break;
        }
    }

    if(v != 0) {
        qnWarning("Provided value '%1' could not be serialized using the values of enum '%2', falling back to integer serialization.", value, m_enumName.isEmpty() ? QStringLiteral("?") : m_enumName);

        if(!target->isEmpty())
            *target += L'|';
        *target += QStringLiteral("0x");
        *target += QString::number(v, 16);
    }
}

bool QnLexicalEnumSerializerData::deserializeFlags(const QString &value, int *target) {
    QStringList names = value.split(L'|');

    *target = 0;

    for(const QString &name: names) {
        QString trimmedName = name.trimmed();
        
        if(trimmedName.isEmpty()) {
            if(names.size() == 1) {
                return true; /* Target is already zero at this point. */
            } else {
                return false;
            }
        }

        int nameValue;
        if(!deserializeEnum(trimmedName, &nameValue)) {
            if(trimmedName.size() >= 3 && trimmedName.startsWith(QStringLiteral("0x"))) {
                bool ok = false;
                nameValue = trimmedName.toInt(&ok, 16);
                if(!ok)
                    return false;
            } else {
                return false;
            }
        }

        *target |= nameValue;
    }

    return true;
}

