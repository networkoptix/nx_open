#ifndef CONTEXT_H
#define CONTEXT_H

#include <QtCore/QObject>

#include <utils/common/instance_storage.h>

class Context: public QObject, public QnInstanceStorage {
    Q_OBJECT
    typedef QObject base_type;

public:
    Context(QObject *parent = NULL);
    virtual ~Context();


private:

};

#endif // CONTEXT_H
