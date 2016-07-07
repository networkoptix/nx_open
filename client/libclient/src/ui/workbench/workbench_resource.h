#ifndef QN_WORKBENCH_RESOURCE_H
#define QN_WORKBENCH_RESOURCE_H

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <core/resource/resource_fwd.h>

class QMimeData;

class QnWorkbenchResource {
public:
    static QStringList resourceMimeTypes();

    static void serializeResources(const QnResourceList &resources, const QStringList &mimeTypes, QMimeData *mimeData);

    static QnResourceList deserializeResources(const QMimeData *mimeData);
};


#endif // QN_WORKBENCH_RESOURCE_H
