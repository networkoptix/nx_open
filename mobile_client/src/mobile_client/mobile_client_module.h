#ifndef MOBILE_CLIENT_MODULE_H
#define MOBILE_CLIENT_MODULE_H

#include <QtCore/QObject>

class QnMobileClientModule : public QObject {
    Q_OBJECT
public:
    QnMobileClientModule(int &argc, char *argv[], QObject *parent = NULL);
};

#endif // MOBILE_CLIENT_MODULE_H
