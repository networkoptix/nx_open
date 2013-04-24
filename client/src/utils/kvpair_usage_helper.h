#ifndef KVPAIR_USAGE_HELPER_H
#define KVPAIR_USAGE_HELPER_H

#include <QtCore/QObject>
#include <QtCore/QString>

#include <api/model/kvpair.h>

#include <core/resource/resource_fwd.h>

struct QnKvPairUsageHelperPrivate;

class QnKvPairUsageHelper: public QObject
{
    Q_OBJECT
public:
    explicit QnKvPairUsageHelper(const QnResourcePtr &resource,
                                 const QString &key,
                                 const QString &defaultValue,
                                 QObject *parent = 0);
    explicit QnKvPairUsageHelper(const QnResourcePtr &resource,
                                 const QString &key,
                                 const quint64 &defaultValue,
                                 QObject *parent = 0);
    ~QnKvPairUsageHelper();

    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr &resource);
    
    QString key() const;
    void setKey(const QString &key);

    QString value() const;
    void setValue(const QString &value);

    quint64 valueAsFlags() const;
    void setFlagsValue(quint64 value);

signals:
    void valueChanged(const QString &value);
private slots:

   void at_connection_replyReceived(int status, const QByteArray &errorString, const QnKvPairs &kvPairs, int handle);
private:
    void load();
    void save();

    QScopedPointer<QnKvPairUsageHelperPrivate> d;
};

#endif // KVPAIR_USAGE_HELPER_H
