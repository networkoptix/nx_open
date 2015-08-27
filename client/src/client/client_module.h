#ifndef QN_CLIENT_MODULE_H
#define QN_CLIENT_MODULE_H

#include <QtCore/QObject>

struct QnStartupParameters;

class QnClientModule: public QObject 
{
    Q_OBJECT

public:
    QnClientModule(const QnStartupParameters &startupParams = QnStartupParameters()
        , QObject *parent = NULL);

    virtual ~QnClientModule();
};


#endif // QN_CLIENT_MODULE_H
