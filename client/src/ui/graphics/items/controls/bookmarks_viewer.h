
#pragma once

#include <core/resource/camera_bookmark_fwd.h>

class QnActionParameters;

/// @class QnBookmarksViewer
/// @brief Shows specified bookmarks one aboveanother in defined position
class QnBookmarksViewer : public QGraphicsWidget
{
    Q_OBJECT
public:
    static QnBookmarksViewer *create(QGraphicsItem *parent);

    virtual ~QnBookmarksViewer();

signals:
    /// @brief Edit action callback
    void editBookmarkClicked(const QnCameraBookmark &bookmark
        , const QnActionParameters& params);

    /// @brief Remove action callback
    void removeBookmarkClicked(const QnCameraBookmark &bookmark
        , const QnActionParameters& params);

public slots:
    /// @brief Updates bookmarks tooltips
    /// @param bookmarks Bookmarks data
    /// @param params Params for the emittance of bookmarks actions (remove\delete)
    void updateBookmarks(const QnCameraBookmarkList &bookmarks
        , const QnActionParameters &params);

    /// @brief Updates position
    /// @param position new position of bookmarks
    /// @param immediately Shows if position should be updated immediately or after inner defined timeout
    void updatePosition(const QPointF &position
        , bool immediately);

    /// @brief immediatelly hides bookmarks
    void hide();

    /// @brief Hides tooltips after inner defined timeout expired
    void hideDelayed();

private:
    QnBookmarksViewer(QGraphicsItem *parent);

private:
    class Impl;
    Impl * const m_impl;
};