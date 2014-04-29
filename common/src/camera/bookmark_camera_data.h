#ifndef __QN_BOOKMARK_CAMERA_DATA_H__
#define __QN_BOOKMARK_CAMERA_DATA_H__

#include <camera/abstract_camera_data.h>
#include <common/common_globals.h>
#include <core/resource/camera_bookmark_fwd.h>

class QnBookmarkCameraData: public QnAbstractCameraData {
public:
    QnBookmarkCameraData();
    QnBookmarkCameraData(const QnCameraBookmarkList &data);
    virtual void append(const QnAbstractCameraDataPtr &other) override;
    virtual QnTimePeriodList dataSource() const override;
    virtual void clear() override;
    virtual bool trimDataSource(qint64 trimTime) override;
    virtual bool isEmpty() const override;

    virtual QnAbstractCameraDataPtr merge(const QVector<QnAbstractCameraDataPtr> &source) override;

    virtual bool operator==(const QnAbstractCameraDataPtr &other) const override;
private:
    QnCameraBookmarkList m_data;
};

#endif // __QN_BOOKMARK_CAMERA_DATA_H__