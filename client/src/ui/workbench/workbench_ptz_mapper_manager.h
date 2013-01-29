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

    const QnPtzSpaceMapper *mapper(const QString &model) const;
    const QnPtzSpaceMapper *mapper(const QnVirtualCameraResourcePtr &resource) const;

    const QList<const QnPtzSpaceMapper *> &mappers() const;

protected:
    void addMapper(const QnPtzSpaceMapper *mapper);
    void removeMapper(const QnPtzSpaceMapper *mapper);

private:
    void loadMappers();
    bool loadMappers(const QString &fileName);
    bool loadMappersInternal(const QString &fileName);

private:
    QList<const QnPtzSpaceMapper *> m_mappers;
    QHash<QString, const QnPtzSpaceMapper *> m_mapperByModel;
};


#endif // QN_WORKBENCH_PTZ_MAPPER_MANAGER_H
