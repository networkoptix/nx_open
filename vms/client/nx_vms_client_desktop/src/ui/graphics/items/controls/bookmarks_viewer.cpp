// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmarks_viewer.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtWidgets/QApplication>

#include <core/resource/camera_bookmark.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/workbench/timeline/bookmark_tooltip.h>
#include <ui/processors/hover_processor.h>
#include <utils/common/delayed.h>

namespace
{
    static constexpr int kUpdateDelay = 150;

    static constexpr qreal kBookmarkFrameWidth = 250;

    enum
    {
        kTagClickedActionEventId = QEvent::User + 1,
        kBookmarkEditActionEventId,
        kBookmarkRemoveActionEventId,
        kBookmarkPlayActionEventId,
        kBookmarkExportActionEventId
    };

    static constexpr int kTooltipTailYOffset = -1;

    ///

    class TagActionEvent : public QEvent
    {
    public:
        TagActionEvent(int eventId, const QString& tag);

        virtual ~TagActionEvent();

        const QString& tag() const;

    private:
        const QString m_tag;
    };

    TagActionEvent::TagActionEvent(int eventId, const QString& tag)
        : QEvent(static_cast<QEvent::Type>(eventId)),
        m_tag(tag)
    {}

    TagActionEvent::~TagActionEvent() {}

    const QString& TagActionEvent::tag() const
    {
        return m_tag;
    }

    /// @class BookmarkActionEvent
    /// @brief Stores parameters for bookmark action generation
    class BookmarkActionEvent : public QEvent
    {
    public:
        BookmarkActionEvent(int eventId, const QnCameraBookmark& bookmark);

        virtual ~BookmarkActionEvent();

        const QnCameraBookmark& bookmark() const;

    private:
        const QnCameraBookmark m_bookmark;
    };

    BookmarkActionEvent::BookmarkActionEvent(int eventId, const QnCameraBookmark& bookmark)
        : QEvent(static_cast<QEvent::Type>(eventId)),
        m_bookmark(bookmark)
    {
    }

    BookmarkActionEvent::~BookmarkActionEvent() {}

    const QnCameraBookmark& BookmarkActionEvent::bookmark() const
    {
        return m_bookmark;
    }
}

///

using namespace nx::vms::client::desktop;

class QnBookmarksViewer::Impl : public QObject
{
public:
    Impl(QWidget* tooltipParentWidget,
        const GetBookmarksFunc& getBookmarksFunc,
        const GetPosOnTimelineFunc& getPosFunc,
        QnBookmarksViewer* owner);

    ~Impl() override;

    ///

    bool readOnly() const;

    void setReadOnly(bool readonly);

    void setAllowExport(bool allowExport);

    void setTargetLocation(qint64 location);

    void updateOnWindowChange();

    ///

    bool isHovered() const;

    void setTimelineHoverProcessor(HoverFocusProcessor* processor);

    ///

    void resetBookmarks();

    QnCameraBookmarkList getBookmarks() const;

private:
    void updatePosition(const QnBookmarksViewer::PosAndBoundsPair& params);

    void updateBookmarks(QnCameraBookmarkList bookmarks);

    bool event(QEvent* event) override;

    void updateLocationInternal(qint64 location);

private:
    const GetBookmarksFunc m_getBookmarks;
    const GetPosOnTimelineFunc m_getPos;

    QnBookmarksViewer* const m_owner;
    HoverFocusProcessor* m_timelineHoverProcessor = nullptr;

    qint64 m_targetLocation;

    QWidget* m_tooltipParentWidget;
    QPointer<workbench::timeline::BookmarkTooltip> m_bookmarkTooltip;
    HoverFocusProcessor* m_bookmarkHoverProcessor;
    QnCameraBookmarkList m_bookmarks;
    bool m_readonly;
    bool m_allowExport = false;

    typedef QScopedPointer<QTimer> QTimerPtr;
    QTimerPtr m_updateDelayedTimer;
    QElapsedTimer m_forceUpdateTimer;
};

enum { kInvalidTimestamp = -1 };

QnBookmarksViewer::Impl::Impl(QWidget* tooltipParentWidget,
    const GetBookmarksFunc& getBookmarksFunc,
    const GetPosOnTimelineFunc& getPosFunc,
    QnBookmarksViewer* owner)

    : QObject(owner),

    m_getBookmarks(getBookmarksFunc),
    m_getPos(getPosFunc),

    m_owner(owner),

    m_targetLocation(kInvalidTimestamp),
    m_tooltipParentWidget(tooltipParentWidget),

    m_bookmarkHoverProcessor(new HoverFocusProcessor(owner)),

    m_bookmarks(),
    m_readonly(false),

    m_updateDelayedTimer(),
    m_forceUpdateTimer()
{
    connect(m_bookmarkHoverProcessor, &HoverFocusProcessor::hoverEntered, this,
        [this]()
        {
            m_updateDelayedTimer.reset();
            m_forceUpdateTimer.invalidate();
        });

    connect(m_bookmarkHoverProcessor, &HoverFocusProcessor::hoverLeft, this,
        [this]()
        {
            resetBookmarks();
        });
}

QnBookmarksViewer::Impl::~Impl()
{
}

bool QnBookmarksViewer::Impl::isHovered() const
{
    return (m_bookmarkHoverProcessor ? m_bookmarkHoverProcessor->isHovered() : false);
}

void QnBookmarksViewer::Impl::setTimelineHoverProcessor(HoverFocusProcessor* processor)
{
    if (m_timelineHoverProcessor == processor)
        return;

    m_timelineHoverProcessor = processor;
    if (!m_timelineHoverProcessor)
        return;

    if (m_bookmarkTooltip)
        m_timelineHoverProcessor->addTargetWidget(m_bookmarkTooltip);
}

bool QnBookmarksViewer::Impl::readOnly() const
{
    return m_readonly;
}

void QnBookmarksViewer::Impl::setReadOnly(bool readonly)
{
    m_readonly = readonly;
}

void QnBookmarksViewer::Impl::setAllowExport(bool allowExport)
{
    if (m_allowExport == allowExport)
        return;

    m_allowExport = allowExport;
    if (m_bookmarkTooltip)
        m_bookmarkTooltip->setAllowExport(m_allowExport);
}

void QnBookmarksViewer::Impl::updateLocationInternal(qint64 location)
{
    m_updateDelayedTimer.reset();

    if (m_targetLocation == location)
        return;

    m_forceUpdateTimer.invalidate();
    const auto newBookmarks = m_getBookmarks(location);
    if (m_bookmarks == newBookmarks)
        return;

    if (newBookmarks.empty())
    {
        resetBookmarks();
        return;
    }

    m_targetLocation = location;
    updateOnWindowChange();
}

void QnBookmarksViewer::Impl::setTargetLocation(qint64 location)
{
    const bool differentBookmarks = (m_getBookmarks(location) != m_bookmarks);
    if (differentBookmarks && !m_forceUpdateTimer.isValid())
        m_forceUpdateTimer.start();

    if (m_bookmarks.empty() || m_forceUpdateTimer.hasExpired(kUpdateDelay))
    {
        // Updates tooltips immediately
        updateLocationInternal(location);
        return;
    }

    const auto updateTimeStamp = [this, location]()
    {
        updateLocationInternal(location);
    };

    m_updateDelayedTimer.reset(executeDelayedParented(updateTimeStamp, kUpdateDelay, this));
}

void QnBookmarksViewer::Impl::updateOnWindowChange()
{
    if (m_targetLocation == kInvalidTimestamp)
        return;

    const auto params = m_getPos(m_targetLocation);
    const auto bounds = params.second;
    if (params.first.isNull() || ((bounds.second - bounds.first) < kBookmarkFrameWidth))
    {
        updateBookmarks(QnCameraBookmarkList());
        return;
    }

    updateBookmarks(m_getBookmarks(m_targetLocation));

    updatePosition(params);
}

void QnBookmarksViewer::Impl::resetBookmarks()
{
    m_updateDelayedTimer.reset();

    if (m_targetLocation == kInvalidTimestamp)
        return;

    m_targetLocation = kInvalidTimestamp;
    updateBookmarks(QnCameraBookmarkList());
}

void QnBookmarksViewer::Impl::updateBookmarks(QnCameraBookmarkList bookmarks)
{
    if (m_bookmarks == bookmarks)
        return;

    m_bookmarks = bookmarks;

    enum { kMaxBookmarksCount = 3 };

    const int bookmarksCount = std::min<int>(m_bookmarks.size(), kMaxBookmarksCount);
    const int bookmarksLeft = m_bookmarks.size() - bookmarksCount;
    const auto& trimmedBookmarks = (bookmarksLeft
        ? m_bookmarks.mid(0, kMaxBookmarksCount) : m_bookmarks);

    if (m_bookmarkTooltip)
    {
        if (m_timelineHoverProcessor)
            m_timelineHoverProcessor->removeTargetWidget(m_bookmarkTooltip);
        m_bookmarkHoverProcessor->removeTargetWidget(m_bookmarkTooltip);
        m_bookmarkTooltip->setVisible(false);
        m_bookmarkTooltip.data()->deleteLater();
    }
    m_bookmarkTooltip.clear();

    if (!trimmedBookmarks.empty())
    {
        if (!m_bookmarkTooltip)
        {
            m_bookmarkTooltip = new workbench::timeline::BookmarkTooltip(
                bookmarks, readOnly(), m_tooltipParentWidget);
            m_bookmarkTooltip->setAllowExport(m_allowExport);
            m_bookmarkTooltip->setVisible(true);
            m_bookmarkTooltip->move(1000, 300);
            if (m_timelineHoverProcessor)
                m_timelineHoverProcessor->addTargetWidget(m_bookmarkTooltip);
            m_bookmarkHoverProcessor->addTargetWidget(m_bookmarkTooltip);

            connect(m_bookmarkTooltip, &workbench::timeline::BookmarkTooltip::playClicked,
                [this](const QnCameraBookmark& bookmark)
                {
                    qApp->postEvent(this, new BookmarkActionEvent(
                        kBookmarkPlayActionEventId, bookmark));
                });
            connect(m_bookmarkTooltip, &workbench::timeline::BookmarkTooltip::editClicked,
                [this](const QnCameraBookmark& bookmark)
                {
                    qApp->postEvent(this, new BookmarkActionEvent(
                        kBookmarkEditActionEventId, bookmark));
                });
            connect(m_bookmarkTooltip, &workbench::timeline::BookmarkTooltip::exportClicked,
                [this](const QnCameraBookmark& bookmark)
                {
                    qApp->postEvent(this, new BookmarkActionEvent(
                        kBookmarkExportActionEventId, bookmark));
                });
            connect(m_bookmarkTooltip, &workbench::timeline::BookmarkTooltip::deleteClicked,
                [this](const QnCameraBookmark& bookmark)
                {
                    qApp->postEvent(this, new BookmarkActionEvent(
                        kBookmarkRemoveActionEventId, bookmark));
                });
            connect(m_bookmarkTooltip, &workbench::timeline::BookmarkTooltip::tagClicked,
                [this](const QString& tag)
                {
                    qApp->postEvent(this, new TagActionEvent(kTagClickedActionEventId, tag));
                });
        }
    }
}

bool QnBookmarksViewer::Impl::event(QEvent* event)
{
    const int eventType = event->type();
    switch(eventType)
    {
        case kTagClickedActionEventId:
        {
            const auto tagActionevent = static_cast<TagActionEvent*>(event);
            emit m_owner->tagClicked(tagActionevent->tag());
            break;
        }
        case kBookmarkEditActionEventId:
        case kBookmarkRemoveActionEventId:
        case kBookmarkPlayActionEventId:
        case kBookmarkExportActionEventId:
        {
            const auto bookmarkActionEvent = static_cast<BookmarkActionEvent*>(event);
            const auto& bookmark = bookmarkActionEvent->bookmark();

            if (eventType == kBookmarkEditActionEventId)
                emit m_owner->editBookmarkClicked(bookmark);
            else if (eventType == kBookmarkRemoveActionEventId)
                emit m_owner->removeBookmarkClicked(bookmark);
            else if (eventType == kBookmarkExportActionEventId)
                emit m_owner->exportBookmarkClicked(bookmark);
            else
                emit m_owner->playBookmark(bookmark);
            break;
        }
        default:
            return QObject::event(event);
    }

    resetBookmarks();
    return true;
}

void QnBookmarksViewer::Impl::updatePosition(const QnBookmarksViewer::PosAndBoundsPair& params)
{
    if (!m_bookmarkTooltip)
        return;

    const QPoint tailPos = params.first.toPoint() + QPoint(0, kTooltipTailYOffset);
    const int minBorderX = (int) params.second.first;
    const int maxBorderX = (int) params.second.second;

    m_bookmarkTooltip->pointTo(tailPos, {QPoint(minBorderX, 0), QPoint(maxBorderX, 0)});
}

QnCameraBookmarkList QnBookmarksViewer::Impl::getBookmarks() const
{
    return m_bookmarks;
}

///

QnBookmarksViewer::QnBookmarksViewer(QWidget* tooltipParentWidget,
    const GetBookmarksFunc& getBookmarksFunc,
    const GetPosOnTimelineFunc& getPosFunc,
    QGraphicsItem* parent)
    : QGraphicsWidget(parent),
    m_impl(new Impl(tooltipParentWidget, getBookmarksFunc, getPosFunc, this))
{
    setGeometry({});
}

QnBookmarksViewer::~QnBookmarksViewer()
{
}

bool QnBookmarksViewer::readOnly() const
{
    return m_impl->readOnly();
}

void QnBookmarksViewer::setReadOnly(bool readonly)
{
    m_impl->setReadOnly(readonly);
}

void QnBookmarksViewer::setAllowExport(bool allowExport)
{
    m_impl->setAllowExport(allowExport);
}

int QnBookmarksViewer::helpTopicAt(const QPointF& /*pos*/) const
{
    return HelpTopic::Id::Bookmarks_Usage;
}

QnCameraBookmarkList QnBookmarksViewer::getDisplayedBookmarks() const
{
    return m_impl->getBookmarks();
}

void QnBookmarksViewer::setTargetLocation(qint64 location)
{
    m_impl->setTargetLocation(location);
}

void QnBookmarksViewer::updateOnWindowChange()
{
    m_impl->updateOnWindowChange();
}

void QnBookmarksViewer::resetBookmarks()
{
    m_impl->resetBookmarks();
}

void QnBookmarksViewer::setTimelineHoverProcessor(HoverFocusProcessor* processor)
{
    m_impl->setTimelineHoverProcessor(processor);
}

bool QnBookmarksViewer::isHovered() const
{
    return m_impl->isHovered();
}
