#ifndef QN_CLIENT_MODULE_H
#define QN_CLIENT_MODULE_H

#include <QtCore/QObject>

class QnCommonModule;

class QnClientModule: public QObject {
    Q_OBJECT
public:
    QnClientModule(int &argc, char **argv, QObject *parent = NULL);
    virtual ~QnClientModule();

private:
    QnCommonModule *m_common;
};


#endif // QN_CLIENT_MODULE_H
