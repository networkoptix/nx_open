
#pragma once

#include <client/client_color_types.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <ui/common/help_topic_queryable.h>

class QnActionParameters;
class HoverFocusProcessor;

/// @class QnBookmarksViewer
/// @brief Shows specified bookmarks one above another in defined position
class QnBookmarksViewer : public QGraphicsWidget, public HelpTopicQueryable
{
    Q_OBJECT
    Q_PROPERTY(QnBookmarkColors colors READ colors WRITE setColors)
    Q_PROPERTY(bool readOnly READ readOnly WRITE setReadOnly)

public:
    typedef QPair<qreal, qreal> Bounds;
    typedef std::pair<QPointF, Bounds> PosAndBoundsPair;
    typedef std::function<QnCameraBookmarkList (qint64 timestamp)> GetBookmarksFunc;
    typedef std::function<PosAndBoundsPair (qint64 timestamp)> GetPosOnTimelineFunc;

    QnBookmarksViewer(const GetBookmarksFunc &getBookmarksFunc
        , const GetPosOnTimelineFunc &getPosFunc
        , QGraphicsItem *parent);

    virtual ~QnBookmarksViewer();

    bool readOnly() const;

    void setReadOnly(bool readonly);

    virtual int helpTopicAt(const QPointF &pos) const override;

signals:
    /// @brief Edit action callback
    /// Closes tooltip after emittance
    void editBookmarkClicked(const QnCameraBookmark &bookmark);

    /// @brief Remove action callback
    /// Closes tooltip after emittance
    void removeBookmarkClicked(const QnCameraBookmark &bookmark);

    /// @brief Tag-clicked action
    /// Closes tooltip after emittance
    void tagClicked(const QString &tag);

    /// @brief Play bookmark action
    /// Closes tooltip after emittance
    void playBookmark(const QnCameraBookmark &bookmark);

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
