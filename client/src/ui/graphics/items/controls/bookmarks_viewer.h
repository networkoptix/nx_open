
#pragma once

#include <core/resource/camera_bookmark_fwd.h>

class QnActionParameters;

/// @class QnBookmarksViewer
/// @brief Shows specified bookmarks one above another in defined position
class QnBookmarksViewer : public QGraphicsWidget
{
    Q_OBJECT
public:
    typedef std::function<QnCameraBookmarkList (qint64 timestamp)> GetBookmarksFunc;
    typedef std::function<QPointF (qint64 timestamp)> GetPosOnTimelineFunc;

    QnBookmarksViewer(const GetBookmarksFunc &getBookmarksFunc
        , const GetPosOnTimelineFunc &getPosFunc
        , QGraphicsItem *parent);

    virtual ~QnBookmarksViewer();

signals:
    /// @brief Edit action callback
    void editBookmarkClicked(const QnCameraBookmark &bookmark);

    /// @brief Remove action callback
    void removeBookmarkClicked(const QnCameraBookmark &bookmark);

public slots:
    /// Set timestamp for bookmarks extraction
    void setTargetTimestamp(qint64 timestamp);

    /// @brief updates position of bookmark(s) when timeline window changed
    void updateOnWindowChange();

    /// @brief clears bookmarks
    void resetBookmarks();

private:
    class Impl;
    Impl * const m_impl;
};
