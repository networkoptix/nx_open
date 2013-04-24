#ifndef QN_API_MODEL_KVPAIR_H
#define QN_API_MODEL_KVPAIR_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMap>

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

typedef QList<QnKvPair> QnKvPairList;
typedef QMap<int, QnKvPairList> QnKvPairs;

#endif // QN_API_MODEL_KVPAIR_H
