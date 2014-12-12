#ifndef QN_CLIENT_MODULE_H
#define QN_CLIENT_MODULE_H

#include <QtCore/QObject>

#include "client_settings.h"

class QnClientModule: public QObject {
    Q_OBJECT
public:
    QnClientModule(int &argc, char **argv, QObject *parent = NULL);
    virtual ~QnClientModule();

private:
	QScopedPointer<QnClientSettings> m_clientSettings;
};


#endif // QN_CLIENT_MODULE_H
