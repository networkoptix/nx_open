#ifndef QN_PTZ_MAPPER_POOL
#define QN_PTZ_MAPPER_POOL

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QHash>

#include "ptz_fwd.h"

// TODO: camera properties file

class QnPtzMapperPool: public QObject {
    Q_OBJECT;
public:
    QnPtzMapperPool(QObject *parent = NULL);
    virtual ~QnPtzMapperPool();

    QnPtzMapperPtr mapper(const QString &model) const;

    bool load(const QString &fileName);

private:
    bool loadInternal(const QString &fileName);

private:
    QHash<QString, QnPtzMapperPtr> m_mapperByModel;
};


#endif // QN_PTZ_MAPPER_POOL
