#ifndef QN_MEDIA_SERVER_MODULE_H
#define QN_MEDIA_SERVER_MODULE_H

#include <QtCore/QObject>

class QnMediaServerModule: public QObject {
    Q_OBJECT;
public:
    QnMediaServerModule(int &argc, char **argv, QObject *parent = NULL);
    virtual ~QnMediaServerModule();
};



#endif // QN_MEDIA_SERVER_MODULE_H
