// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsWidget>

#include <core/resource/camera_bookmark_fwd.h>
#include <ui/common/help_topic_queryable.h>

class HoverFocusProcessor;

/** Shows specified bookmarks one above another in defined position. */
class QnBookmarksViewer: public QGraphicsWidget, public HelpTopicQueryable
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ readOnly WRITE setReadOnly)

public:
    typedef QPair<qreal, qreal> Bounds;
    typedef std::pair<QPointF, Bounds> PosAndBoundsPair;
    typedef std::function<QnCameraBookmarkList (qint64 location)> GetBookmarksFunc;
    typedef std::function<PosAndBoundsPair (qint64 location)> GetPosOnTimelineFunc;

    QnBookmarksViewer(QWidget* tooltipParentWidget,
        const GetBookmarksFunc& getBookmarksFunc,
        const GetPosOnTimelineFunc& getPosFunc,
        QGraphicsItem* parent);

    virtual ~QnBookmarksViewer();

    bool readOnly() const;

    void setReadOnly(bool readonly);

    void setAllowExport(bool allowExport);
    void setAllowShare(bool allowShare);

    virtual int helpTopicAt(const QPointF& pos) const override;

    QnCameraBookmarkList getDisplayedBookmarks() const;

signals:
    void editBookmarkClicked(const QnCameraBookmark& bookmark);
    void removeBookmarkClicked(const QnCameraBookmark& bookmark);
    void tagClicked(const QString& tag);
    void playBookmark(const QnCameraBookmark& bookmark);
    void exportBookmarkClicked(const QnCameraBookmark& bookmark);
    void shareBookmarkClicked(const QnCameraBookmark& bookmark);

public slots:
    /**
     * Sets the abstract location used for bookmark extraction, it can be a timeline coordinate
     * or a timestamp. The location is interpreted by the getBookmarksFunc/getPosFunc callbacks
     * passed to the constructor.
     */
    void setTargetLocation(qint64 location);

    /** Updates position of bookmarks when timeline window changed. */
    void updateOnWindowChange();

    /** Clears bookmarks. */
    void resetBookmarks();

public slots:
    void setTimelineHoverProcessor(HoverFocusProcessor* processor);

    bool isHovered() const;

private:
    class Impl;
    Impl* const m_impl;
};
