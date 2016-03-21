#ifndef __QN_ABSTRACT_CAMERA_DATA_H__
#define __QN_ABSTRACT_CAMERA_DATA_H__

#include <common/common_globals.h>

#include <camera/data/camera_data_fwd.h>

class QnTimePeriod;
class QnTimePeriodList;

/**
 * @brief The QnAbstractCameraData class            Interface class for storing and handling camera archive metadata.
 */
class QnAbstractCameraData {
public:
    virtual ~QnAbstractCameraData() {}

    /**
     * @brief clone                                 Deep copy of the object.
     * @return                                      Shared pointer to the new data object.
     */
    virtual QnAbstractCameraDataPtr clone() const = 0;

    /**
     * @brief isEmpty                               Check that there is some data in the struct.
     * @return                                      True if there is at least one piece of data.
     */
    virtual bool isEmpty() const = 0;
    
    /**
     * @brief update                                Append other piece of data.
     * @param other                                 Other data struct.
     */
    virtual void update(const QnAbstractCameraDataPtr &other, const QnTimePeriod &updatedPeriod) = 0;

    /**
     * @brief mergeInto                             Append several pieces of data at once.
     * @param other                                 List of data structs to append.
     */
    virtual void mergeInto(const QList<QnAbstractCameraDataPtr> &other) = 0;

    /**
     * @brief trim                                  Trim the last data source period if it is not finished.
     * @param trimTime                              Value to be set as the end time of the last time period.
     * @return                                      True if the data source was modified, false otherwise.
     */
    virtual bool trim(qint64 trimTime) = 0;

    /**
     * @brief clear                                 Remove all data from the struct.
     */
    virtual void clear() = 0;

    /**
     * @brief contains                              Check that some piece of data is fully covered with stored data.
     * @param data                                  Checked data.
     * @return                                      True if the data contains all the checked data.
     */
    virtual bool contains(const QnAbstractCameraDataPtr &data) const = 0;

    /**
     * @brief dataSource                            Get the source time for the stored metadata.
     * @return                                      Sorted list of time periods containing the metadata.
     */
    virtual QnTimePeriodList dataSource() const = 0;
};

Q_DECLARE_METATYPE(QnAbstractCameraDataPtr)

#endif // __QN_ABSTRACT_CAMERA_DATA_H__
