#ifndef QN_API_MODEL_KVPAIR_H
#define QN_API_MODEL_KVPAIR_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMetaType>

#include <api/api_fwd.h>

class QnKvPair {
public:
    QnKvPair();
    QnKvPair(const QString& name, const QString& value, bool _isPredefinedParam = false);
    QnKvPair(const QString& name, const int value, bool _isPredefinedParam = false);
    QnKvPair(const QString& name, const bool value, bool _isPredefinedParam = false);
    ~QnKvPair() {}

    void setName(const QString& name);
    const QString& name() const;

    void setValue(const QString& value);
    const QString& value() const;

    //!Returns \a true for parameters, registered in xml file, \a false - for resource "properties" (same but not registered in xml)
    bool isPredefinedParam() const;

private:
    QString m_name;
    QString m_value;
    bool m_isPredefinedParam;
};

Q_DECLARE_METATYPE(QnKvPair)
Q_DECLARE_METATYPE(QnKvPairList)
Q_DECLARE_METATYPE(QnKvPairListsById)

#endif // QN_API_MODEL_KVPAIR_H
