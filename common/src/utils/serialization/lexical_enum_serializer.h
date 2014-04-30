#ifndef QN_LEXICAL_ENUM_SERIALIZER_H
#define QN_LEXICAL_ENUM_SERIALIZER_H

#include "lexical.h"

#include <QtCore/QHash>
#include <QtCore/QString>

struct QMetaObject;

class QnLexicalEnumSerializerData {
public:
    QnLexicalEnumSerializerData() {}

    void insert(int value, const QString &name);
    void insert(const QMetaObject *metaObject, const char *enumName);

    void serialize(int value, QString *target);
    bool deserialize(const QString &value, int *target);

    void serializeFlags(int value, QString *target);
    bool deserializeFlags(const QString &value, int *target);

private:
    QHash<int, QString> m_nameByValue;
    QHash<QString, int> m_valueByName;
    QString m_enumName;
};


template<class Enum>
class QnLexicalEnumSerializer: public QnLexicalSerializer {
    static_assert(sizeof(Enum) <= sizeof(int), "Enumeration types larger than int in size are not supported.");
    typedef QnLexicalSerializer base_type;

public:
    QnLexicalEnumSerializer(): 
        base_type(qMetaTypeId<Enum>()) 
    {}

    QnLexicalEnumSerializer(const QnLexicalEnumSerializerData &data): 
        base_type(qMetaTypeId<Enum>()),
        m_data(data)
    {}

protected:
    virtual void serializeInternal(const void *value, QString *target) const override {
        m_data.serialize(*static_cast<const Enum *>(value), target);
    }

    virtual bool deserializeInternal(const QString &value, void *target) const override {
        int tmp;
        if(!m_data.deserialize(value, &tmp))
            return false;
        *static_cast<Enum *>(target) = static_cast<Enum>(tmp);
    }

private:
    QnLexicalEnumSerializerData m_data;
};




#endif // QN_LEXICAL_ENUM_SERIALIZER_H
