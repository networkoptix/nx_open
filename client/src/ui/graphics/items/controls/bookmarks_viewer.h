
#pragma once

#include <client/client_color_types.h>
#include <core/resource/camera_bookmark_fwd.h>

class QnActionParameters;
class HoverFocusProcessor;

/// @class QnBookmarksViewer
/// @brief Shows specified bookmarks one above another in defined position
class QnBookmarksViewer : public QGraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(QnBookmarkColors colors READ colors WRITE setColors)

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

public slots:
    void setHoverProcessor(HoverFocusProcessor *processor);

    bool isHovered() const;

public:
    const QnBookmarkColors &colors() const;

    void setColors(const QnBookmarkColors &colors);

private:
    class Impl;
    Impl * const m_impl;
};
