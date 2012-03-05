#ifndef QN_WORKBENCH_RESOURCE_H
#define QN_WORKBENCH_RESOURCE_H

#include <QString>
#include <QByteArray>
#include <core/resource/resource_fwd.h>

class QnWorkbenchResource {
public:
    static QString resourcesMime();

    static QByteArray serializeResources(const QnResourceList &resources);

    static QnResourceList deserializeResources(const QByteArray &data);

};


#endif // QN_WORKBENCH_RESOURCE_H
