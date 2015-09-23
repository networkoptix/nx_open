#ifndef QN_CLIENT_MODULE_H
#define QN_CLIENT_MODULE_H

#include <QtCore/QObject>

#include <client/client_startup_parameters.h>

class QnClientModule: public QObject 
{
    Q_OBJECT

public:
    QnClientModule(const QnStartupParameters &startupParams = QnStartupParameters()
        , QObject *parent = NULL);

    virtual ~QnClientModule();
};


#endif // QN_CLIENT_MODULE_H
