#ifndef QN_ENUM_NAME_MAPPER_H
#define QN_ENUM_NAME_MAPPER_H

#include <cassert>

#include <boost/preprocessor/stringize.hpp>

#include <QtCore/QMetaObject>

#include <utils/common/warnings.h>

class QnEnumNameMapper {
public:
    QnEnumNameMapper() {}

    QnEnumNameMapper(const QMetaObject *metaObject, const char *enumName) {
        assert(metaObject && enumName);

        int index = metaObject->indexOfEnumerator(enumName);
        if(index == -1) {
            qnWarning("Class '%1' had no enum '%2'.", metaObject->className(), enumName);
            return;
        }

        QMetaEnum enumerator = metaObject->enumerator(index);
        for(int i = 0; i < enumerator.keyCount(); i++)
            addValue(enumerator.value(i), QLatin1String(enumerator.key(i)));
    }

    void addValue(int value, const QString &name) {
        assert(m_valueByName.contains(name) ? m_valueByName.value(name) == value : true);
        
        m_valueByName[name] = value;
        if(!m_nameByValue.contains(value))
            m_nameByValue[value] = name;
    }

    int value(const QString &name, int defaultValue = -1) const {
        return m_valueByName.value(name, defaultValue);
    }

    QString name(int value, const QString &defaultValue = QString()) const {
        return m_nameByValue.value(value, defaultValue);
    }

private:
    QHash<int, QString> m_nameByValue;
    QHash<QString, int> m_valueByName;
};


#define qnEnumNameMapper(SCOPE, ENUM)                                           \
    QnEnumNameMapper(&SCOPE::staticMetaObject, BOOST_PP_STRINGIZE(ENUM))


#endif // QN_ENUM_NAME_MAPPER_H
