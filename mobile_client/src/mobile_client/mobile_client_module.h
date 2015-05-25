#ifndef MOBILE_CLIENT_MODULE_H
#define MOBILE_CLIENT_MODULE_H

#include <QtCore/QObject>

class QnMobileClientModule : public QObject {
    Q_OBJECT
public:
    QnMobileClientModule(QObject *parent = NULL);
    ~QnMobileClientModule();
};

#endif // MOBILE_CLIENT_MODULE_H
