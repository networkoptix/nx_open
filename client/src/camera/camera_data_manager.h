#ifndef CAMERA_DATA_MANAGER_H
#define CAMERA_DATA_MANAGER_H

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

class QnCachingCameraDataLoader;

class QnCameraDataManager : public QObject
{
    Q_OBJECT
public:
    explicit QnCameraDataManager(QObject *parent = 0);
    virtual ~QnCameraDataManager();
   
    QnCachingCameraDataLoader *loader(const QnMediaResourcePtr &resource, bool createIfNotExists = true);

    void clearCache();
signals:
    void periodsChanged(const QnMediaResourcePtr &resource, Qn::TimePeriodContent type, qint64 startTimeMs);
private:
    mutable QHash<QnMediaResourcePtr, QnCachingCameraDataLoader *> m_loaderByResource;
};

#endif // CAMERA_DATA_MANAGER_H