#ifndef QN_OBJECT_H
#define QN_OBJECT_H

#include <QtCore/QObject>

#include "connective.h"

class QnObject: public Connective<QObject> {
    Q_OBJECT;

    typedef Connective<QObject> base_type;
public:
    QnObject(QObject *parent = NULL): base_type(parent) {}
};

#endif // QN_OBJECT_H
