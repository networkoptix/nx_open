#ifndef QN_OBJECT_H
#define QN_OBJECT_H

#include <QtCore/QObject>

#include "adl_connective.h"

class QnObject: public AdlConnective<QObject> {
    Q_OBJECT;

    typedef AdlConnective<QObject> base_type;
public:
    QnObject(QObject *parent = NULL): base_type(parent) {}
};

#endif // QN_OBJECT_H
