#ifndef QN_API_MODEL_KVPAIR_H
#define QN_API_MODEL_KVPAIR_H

#include <QtCore/QString>
#include <QtCore/QList>

class QnKvPair {
public:
    QnKvPair() {}
    QnKvPair(const QString& name, const QString& value);
    ~QnKvPair() {}

    void setName(const char* name);
    void setName(const QString& name);
    const QString& name() const;

    void setValue(const char* value);
    void setValue(const QString& value);
    const QString& value() const;

private:
    QString m_name;
    QString m_value;
};

typedef QList<QnKvPair> QnKvPairList;

#endif // QN_API_MODEL_KVPAIR_H
