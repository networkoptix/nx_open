#ifndef QN_API_MODEL_KVPAIR_H
#define QN_API_MODEL_KVPAIR_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include "utils/common/id.h"


class QnKvPair {
public:
    QnKvPair() {}
    QnKvPair(const QString& name, const QString& value);
    QnKvPair(const QString& name, const int value);
    QnKvPair(const QString& name, const bool value);
    ~QnKvPair() {}

    void setName(const QString& name);
    const QString& name() const;

    void setValue(const QString& value);
    const QString& value() const;

private:
    QString m_name;
    QString m_value;
};

// TODO: #Elric bad naming
typedef QList<QnKvPair> QnKvPairList;
typedef QMap<QnId, QnKvPairList> QnKvPairListsById;

Q_DECLARE_METATYPE(QnKvPair)
Q_DECLARE_METATYPE(QnKvPairList)
Q_DECLARE_METATYPE(QnKvPairListsById)

#endif // QN_API_MODEL_KVPAIR_H
