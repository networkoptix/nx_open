#ifndef CONTEXT_H
#define CONTEXT_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <utils/common/instance_storage.h>

#include <nx_ec/ec_api_fwd.h>

class Context: public QObject, public QnInstanceStorage {
    Q_OBJECT
    typedef QObject base_type;

public:
    Context(QObject *parent = NULL);
    virtual ~Context();

private:
    QScopedPointer<ec2::AbstractECConnectionFactory> m_connectionFactory;
};

Q_DECLARE_METATYPE(Context *)

#endif // CONTEXT_H
