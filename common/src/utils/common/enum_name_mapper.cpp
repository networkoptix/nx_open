#include "enum_name_mapper.h"

QnEnumNameMapper::QnEnumNameMapper(const QMetaObject *metaObject, const char *enumName) {
    assert(metaObject && enumName);

    int index = metaObject->indexOfEnumerator(enumName);
    if(index == -1) {
        qnWarning("Class '%1' has no enum '%2'.", metaObject->className(), enumName);
        return;
    }

    QMetaEnum enumerator = metaObject->enumerator(index);
    for(int i = 0; i < enumerator.keyCount(); i++)
        insert(enumerator.value(i), QLatin1String(enumerator.key(i)));
}

void QnEnumNameMapper::insert(int value, const char *name) {
    insert(value, QLatin1String(name));
}

void QnEnumNameMapper::insert(int value, const QString &name) {
    if(!m_valueByName.contains(name))
        m_valueByName[name] = value;
    if(!m_nameByValue.contains(value))
        m_nameByValue[value] = name;
}

int QnEnumNameMapper::value(const QString &name, int defaultValue) const {
    return m_valueByName.value(name, defaultValue);
}

int QnEnumNameMapper::value(const QString &name, bool *ok) const {
    auto pos = m_valueByName.find(name);
    if(pos == m_valueByName.end()) {
        if(ok)
            *ok = false;
        return -1;
    } else {
        if(ok)
            *ok = true;
        return *pos;
    }
}

QString QnEnumNameMapper::name(int value, const QString &defaultValue) const {
    return m_nameByValue.value(value, defaultValue);
}


class QtStaticMetaObject: public QObject {
public:
    using QObject::staticQtMetaObject;
};

const QMetaObject &Qt::staticMetaObject = QtStaticMetaObject::staticQtMetaObject;
