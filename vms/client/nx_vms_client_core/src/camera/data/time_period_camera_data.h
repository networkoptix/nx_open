// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef __QN_TIME_PERIOD_CAMERA_DATA_H__
#define __QN_TIME_PERIOD_CAMERA_DATA_H__

#include <camera/data/abstract_camera_data.h>

#include <recording/time_period_list.h>

/**
 * @brief The QnTimePeriodCameraData class          Class for storing and handling camera archive time periods.
 */
class QnTimePeriodCameraData: public QnAbstractCameraData {
public:
    QnTimePeriodCameraData();
    QnTimePeriodCameraData(const QnTimePeriodList &data);

    QnAbstractCameraDataPtr clone() const override;

    /**
     * @brief isEmpty                               Check that the list is not empty.
     * @return                                      True if there is at least one time period.
     */
    virtual bool isEmpty() const override;

    /**
     * @brief append                                Append other set of time periods.
     * @param other                                 Other data struct.
     */
    virtual void update(const QnAbstractCameraDataPtr &other, const QnTimePeriod &updatedPeriod) override;

    /**
     * @brief append                                Append several sets of time periods at once.
     * @param other                                 List of data structs to append.
     */
    virtual void mergeInto(const QList<QnAbstractCameraDataPtr> &other) override;

    /**
     * @brief append                                Append other set of time periods.
     * @param other                                 Plain list of time periods.
     */
    void update(const QnTimePeriodList &other, const QnTimePeriod &updatedPeriod);

    /**
     * @brief clear                                 Remove all bookmarks from the struct.
     */
    virtual void clear() override;

    /**
     * @brief contains                              Check that some time periods are contained in the stored data.
     * @param data                                  Checked data.
     * @return                                      True if the data contains all the checked time periods.
     */
    virtual bool contains(const QnAbstractCameraDataPtr &data) const override;

    /**
     * @brief dataSource                            Get the source time for the stored time periods.
     * @return                                      Sorted list of time periods.
     */
    virtual QnTimePeriodList dataSource() const override;

    /**
     * @brief trim                                  Trim the last time period if it is not finished.
     * @param trimTime                              Value to be set as the end time of the last time period.
     * @return                                      True if the list was modified, false otherwise.
     */
    virtual bool trim(qint64 trimTime) override;
private:
    /** List of the stored time periods. */
    QnTimePeriodList m_data;
};

#endif // __QN_TIME_PERIOD_CAMERA_DATA_H__
