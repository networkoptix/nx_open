#ifndef QN_PTZ_MAPPER_POOL
#define QN_PTZ_MAPPER_POOL

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QHash>

class QnPtzSpaceMapper;

/**
 * Pool for manually created ptz mappers.
 */
class QnPtzMapperPool: public QObject {
    Q_OBJECT;
public:
    QnPtzMapperPool(QObject *parent = NULL);
    virtual ~QnPtzMapperPool();

    const QnPtzSpaceMapper *mapper(const QString &model) const;

    bool load(const QString &fileName);

protected:
    void addMapper(const QnPtzSpaceMapper *mapper);
    void removeMapper(const QnPtzSpaceMapper *mapper);

private:
    bool loadInternal(const QString &fileName);

private:
    QList<const QnPtzSpaceMapper *> m_mappers;
    QHash<QString, const QnPtzSpaceMapper *> m_mapperByModel;
};


#endif // QN_PTZ_MAPPER_POOL
