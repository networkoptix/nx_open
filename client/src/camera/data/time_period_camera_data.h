#ifndef __QN_TIME_PERIOD_CAMERA_DATA_H__
#define __QN_TIME_PERIOD_CAMERA_DATA_H__

#include <camera/data/abstract_camera_data.h>
#include <common/common_globals.h>
#include <recording/time_period_list.h>

class QnTimePeriodCameraData: public QnAbstractCameraData {
public:
    QnTimePeriodCameraData();
    QnTimePeriodCameraData(const QnTimePeriodList &data);

    virtual void append(const QnAbstractCameraDataPtr &other) override;
    void append(const QnTimePeriodList &other);

    QnTimePeriodList data() const;
    virtual QnTimePeriodList dataSource() const override;
    virtual void clear() override;

    bool trim(qint64 trimTime);

    virtual QnAbstractCameraDataPtr merge(const QVector<QnAbstractCameraDataPtr> &source) override;

    virtual bool contains(const QnAbstractCameraDataPtr &other) const override;
private:
    QnTimePeriodList m_data;
};

#endif // __QN_TIME_PERIOD_CAMERA_DATA_H__