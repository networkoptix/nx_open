#include "lexical_enum.h"

#include <cassert>

#include <QtCore/QMetaObject>
#include <QtCore/QMetaEnum>

#include <utils/common/warnings.h>

#include "lexical_functions.h"


// -------------------------------------------------------------------------- //
// QnLexicalEnumSerializerData
// -------------------------------------------------------------------------- //
void QnEnumLexicalSerializerData::load(const QMetaObject *metaObject, const char *enumName) {
    assert(metaObject && enumName);

    clear();

    int index = metaObject->indexOfEnumerator(enumName);
    assert(index >= 0); /* Getting an assert here? Do your class actually contain the provided enumeration? Maybe you've forgotten to use Q_ENUMS? */

    QMetaEnum enumerator = metaObject->enumerator(index);
    m_flagged = enumerator.isFlag();

    QString scope = QLatin1String(metaObject->className()) + QStringLiteral("::");
    for(int i = 0; i < enumerator.keyCount(); i++) {
        insert(enumerator.value(i), QLatin1String(enumerator.key(i)));
        insert(enumerator.value(i), scope + QLatin1String(enumerator.key(i))); /* Also support scoped names when deserializing. */
    }

    m_enumName = scope + QLatin1String(enumName);
}

void QnEnumLexicalSerializerData::insert(int value, const QString &name) {
    QString lowerName = name.toLower();

    if(!m_valueByName.contains(name))
        m_valueByName[name] = value;
    if(!m_valueByLowerName.contains(lowerName))
        m_valueByLowerName[lowerName] = value;
    if(!m_nameByValue.contains(value))
        m_nameByValue[value] = name;
}

void QnEnumLexicalSerializerData::insert(int value, const char *name) {
    insert(value, QLatin1String(name));
}

void QnEnumLexicalSerializerData::clear() {
    m_nameByValue.clear();
    m_valueByName.clear();
    m_enumName.clear();
    m_flagged = false;
}

bool QnEnumLexicalSerializerData::isFlagged() const {
    return m_flagged;
}

void QnEnumLexicalSerializerData::setFlagged(bool flagged) {
    m_flagged = flagged;
}

bool QnEnumLexicalSerializerData::isNumeric() const {
    return m_numeric;
}

void QnEnumLexicalSerializerData::setNumeric(bool numeric) {
    m_numeric = numeric;
}

Qt::CaseSensitivity QnEnumLexicalSerializerData::caseSensitivity() const {
    return m_caseSensitivity;
}

void QnEnumLexicalSerializerData::setCaseSensitivity(Qt::CaseSensitivity caseSensitivity) {
    m_caseSensitivity = caseSensitivity;
}

void QnEnumLexicalSerializerData::serializeEnum(int value, QString *target) const {
    if(m_numeric) {
        QnLexical::serialize(value, target);
        return;
    }

    /* Return empty string in case of failure. */
    *target = m_nameByValue.value(value); 
}

bool QnEnumLexicalSerializerData::deserializeEnum(const QString &value, int *target) const {
    /* Try numeric first. */
    if(m_numeric)
        if(QnLexical::deserialize(value, target))
            return true;

    if(m_caseSensitivity == Qt::CaseSensitive) {
        return deserializeEnumInternal(m_valueByName, value, target);
    } else {
        return deserializeEnumInternal(m_valueByLowerName, value.toLower(), target);
    }
}

bool QnEnumLexicalSerializerData::deserializeEnumInternal(const QHash<QString, int> &hash, const QString &value, int *target) const {
    auto pos = hash.find(value);
    if(pos == hash.end())
        return false;

    *target = *pos;
    return true;
}

void QnEnumLexicalSerializerData::serializeFlags(int value, QString *target) const {
    if(m_numeric) {
        QnLexical::serialize(value, target);
        return;
    }

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

bool QnEnumLexicalSerializerData::deserializeFlags(const QString &value, int *target) const {
    /* Try numeric first. */
    if(m_numeric)
        if(QnLexical::deserialize(value, target))
            return true;

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

void QnEnumLexicalSerializerData::serialize(int value, QString *target) const {
    m_flagged ? serializeFlags(value, target) : serializeEnum(value, target);

}

bool QnEnumLexicalSerializerData::deserialize(const QString &value, int *target) const {
    return m_flagged ? deserializeFlags(value, target) : deserializeEnum(value, target);
}



// -------------------------------------------------------------------------- //
// QtStaticMetaObject
// -------------------------------------------------------------------------- //
class QtStaticMetaObject: public QObject {
public:
    using QObject::staticQtMetaObject;
};

const QMetaObject &Qt::staticMetaObject = QtStaticMetaObject::staticQtMetaObject;
