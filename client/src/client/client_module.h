#ifndef QN_CLIENT_MODULE_H
#define QN_CLIENT_MODULE_H

#include <QtCore/QObject>

class QnClientModule: public QObject {
    Q_OBJECT
public:
    QnClientModule(bool forceLocalSettings = false, QObject *parent = NULL);
    virtual ~QnClientModule();
};


#endif // QN_CLIENT_MODULE_H
