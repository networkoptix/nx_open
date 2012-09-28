#ifndef QN_COMMON_MODULE_H
#define QN_COMMON_MODULE_H

#include <QtCore/QObject>

class QnCommonModule: public QObject {
    Q_OBJECT
public:
    QnCommonModule(int &argc, char **argv, QObject *parent = NULL);
    virtual ~QnCommonModule();
};

#endif // QN_COMMON_MODULE_H

