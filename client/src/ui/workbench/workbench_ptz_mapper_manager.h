#ifndef QN_WORKBENCH_PTZ_MAPPER_MANAGER_H
#define QN_WORKBENCH_PTZ_MAPPER_MANAGER_H

#include <QtCore/QObject>

#include <utils/common/space_mapper.h>
#include <core/resource/resource_fwd.h>

#include "workbench_context_aware.h"

class QnWorkbenchPtzMapperManager: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchPtzMapperManager(QObject *parent = NULL);
    virtual ~QnWorkbenchPtzMapperManager();

    QnPtzSpaceMapper *mapper(const QString &model) const;
    QnPtzSpaceMapper *mapper(const QnVirtualCameraResourcePtr &resource) const;

    const QList<QnPtzSpaceMapper *> &mappers() const;

protected:
    void addMapper(QnPtzSpaceMapper *mapper);
    void removeMapper(QnPtzSpaceMapper *mapper);

private:
    void loadMappers();
    bool loadMappers(const QString &fileName);
    bool loadMappersInternal(const QString &fileName);

private:
    QList<QnPtzSpaceMapper *> m_mappers;
    QHash<QString, QnPtzSpaceMapper *> m_mapperByModel;
};


#endif // QN_WORKBENCH_PTZ_MAPPER_MANAGER_H
