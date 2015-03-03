
#pragma once

#include <core/resource/camera_bookmark_fwd.h>

class QnActionParameters;

class QnBookmarksViewer : public QGraphicsWidget
{
    Q_OBJECT
public:
    static QnBookmarksViewer *create(QGraphicsItem *parent);

    virtual ~QnBookmarksViewer();

signals:
    void editBookmarkClicked(const QnCameraBookmark &bookmark
        , const QnActionParameters& params);

    void removeBookmarkClicked(const QnCameraBookmark &bookmark
        , const QnActionParameters& params);

public slots:
    void updateBookmarks(const QnCameraBookmarkList &bookmarks
        , const QnActionParameters &params);

    void updatePosition(const QPointF &basePosition
        , bool immediately);

    void hide();

    void leaveTimeLine();

private:
    QnBookmarksViewer(QGraphicsItem *parent);

private:
    class Impl;
    Impl * const m_impl;
};