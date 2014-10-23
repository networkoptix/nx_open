#ifndef __QN_BOOKMARK_CAMERA_DATA_H__
#define __QN_BOOKMARK_CAMERA_DATA_H__

#include <camera/data/abstract_camera_data.h>
#include <common/common_globals.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <recording/time_period_list.h>

/**
 * @brief The QnBookmarkCameraData class            Class for storing and handling camera archive bookmarks.
 */
class QnBookmarkCameraData: public QnAbstractCameraData {
public:
    QnBookmarkCameraData();
    QnBookmarkCameraData(const QnCameraBookmarkList &data);

    QnAbstractCameraDataPtr clone() const override;

    /**
     * @brief isEmpty                               Check that the list is not empty.
     * @return                                      True if there is at least one bookmark.
     */
    virtual bool isEmpty() const override;

    /**
     * @brief append                                Append other set of bookmarks.
     * @param other                                 Other data struct, must be instance of QnBookmarkCameraData.
     */
    virtual void append(const QnAbstractCameraDataPtr &other) override;

    /**
     * @brief append                                Append several sets of bookmarks at once.
     * @param other                                 List of data structs to append, each of them must be instance of QnBookmarkCameraData.
     */
    virtual void append(const QList<QnAbstractCameraDataPtr> &other) override;

    /**
     * @brief clear                                 Remove all bookmarks from the struct.
     */
    virtual void clear() override;

    /**
     * @brief contains                              Check that some bookmarks are contained in the stored data (only GUIDs are checked).
     * @param data                                  Checked data, must be instance of QnBookmarkCameraData.
     * @return                                      True if the data contains all the checked bookmarks.
     */
    virtual bool contains(const QnAbstractCameraDataPtr &data) const override;

    /**
     * @brief dataSource                            Get the source time for the stored bookmarks.
     * @return                                      Sorted list of time periods containing the bookmarks.
     */
    virtual QnTimePeriodList dataSource() const override;

    /**
     * @brief trim                                  Trim the last time period if it is not finished.
     * @param trimTime                              Value to be set as the end time of the last time period.
     * @return                                      True if the list was modified, false otherwise.
     */
    virtual bool trim(qint64 trimTime) override;

    /**
     * @brief find                                  Find the longest bookmark covering the certain moment of time.
     * @param position                              Time in milliseconds since epoch.
     * @return                                      Longest bookmark found at that time; empty bookmark if nothing is found.
     */
    QnCameraBookmark find(const qint64 position) const;

    /**
     * @brief data                                  Get raw data list.
     * @return                                      List of all stored bookmarks sorted by their start time.
     */
    QnCameraBookmarkList data() const;

    /**
     * @brief updateBookmark                        Update bookmark in the list.
     * @param bookmark                              Updated bookmark.
     */
    void updateBookmark(const QnCameraBookmark &bookmark);

    /**
     * @brief removeBookmark                        Remove bookmark from the list.
     * @param bookmark                              Value to remove (only GUID is used).
     */
    void removeBookmark(const QnCameraBookmark & bookmark);
private:

    /**
     * @brief updateDataSource                      Rebuild bookmark time periods cache. Full rebuild is required
     *                                              in some cases because bookmarks can overlap.
     */
    void updateDataSource();

    /** List of bookmarks, sorted by their start time. Sorting is not stable. */
    QnCameraBookmarkList m_data;

    /** Cache of bookmark time periods, merged. */
    QnTimePeriodList m_dataSource;
};

#endif // __QN_BOOKMARK_CAMERA_DATA_H__
