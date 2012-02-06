#ifndef QN_VIEW_DRAG_AND_DROP_H
#define QN_VIEW_DRAG_AND_DROP_H

#include <QList>
#include <QByteArray>
#include <core/resource/resource.h>

QString resourcesMime();

QByteArray serializeResources(const QnResourceList &resources);

QnResourceList deserializeResources(const QByteArray &data);

#endif //QN_VIEW_DRAG_AND_DROP_H
