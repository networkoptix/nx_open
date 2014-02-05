#ifndef KVPAIR_USAGE_HELPER_H
#define KVPAIR_USAGE_HELPER_H

#include <QtCore/QObject>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>

#include <api/model/kvpair.h>
#include "nx_ec/impl/ec_api_impl.h"

// TODO: #Elric remove this header.

struct QnKvPairUsageHelperPrivate;

class QnAbstractKvPairUsageHelper: public QObject {
    Q_OBJECT
public:
    ~QnAbstractKvPairUsageHelper();

    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr &resource);
    
    QString key() const;
    void setKey(const QString &key);

protected:
    explicit QnAbstractKvPairUsageHelper(const QnResourcePtr &resource,
                                 const QString &key,
                                 const QString &defaultValue,
                                 QObject *parent = 0);

    QString innerValue() const;
    void setInnerValue(const QString &value);
    virtual void innerValueChanged(const QString &value) = 0;

private slots:
   void at_connection_replyReceived(int handle, ec2::ErrorCode err, const QnKvPairListsById &kvPairs);

private:
    void load();
    void save();

    QScopedPointer<QnKvPairUsageHelperPrivate> d;
};

class QnStringKvPairUsageHelper: public QnAbstractKvPairUsageHelper {
    Q_OBJECT

    typedef QnAbstractKvPairUsageHelper base_type;
public:
    explicit QnStringKvPairUsageHelper(const QnResourcePtr &resource,
                                 const QString &key,
                                 const QString &defaultValue,
                                 QObject *parent = 0);
    ~QnStringKvPairUsageHelper();

    QString value() const;
    void setValue(const QString &value);

signals:
    void valueChanged(const QString &value);

protected:
    virtual void innerValueChanged(const QString &value) override;
};


#endif // KVPAIR_USAGE_HELPER_H
