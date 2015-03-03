
#include "bookmarks_viewer.h"

#include <core/resource/camera_bookmark.h>
#include <ui/processors/hover_processor.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>

#include <ui/actions/action_parameters.h>

namespace
{
    enum 
    {
        kTimerInvalidId = 0
        , kTimerPeriod = 700
    };

    enum { kToolTipHidingTime = 3000 };
    enum 
    {
        kBookmarkFrameWidth = 250
        , kHalfBookmarkFrameWidth = kBookmarkFrameWidth / 2
    };

    const QString kEditActionAnchorName = lit("e");
    const QString kRemoveActoinAnchorName = lit("r");
    const QString kEditLabelCaption = QObject::tr("Edit");
    const QString kRemoveLabelCaption = QObject::tr("Remove");
    const QString kLinkTemplate = lit("<a href = \"%1\">%2</a>");
    const QString kDelimitter = lit(" ");
    /// 

    void deleteGraphicItem(QGraphicsItem *item)
    {
        item->setParentItem(nullptr);
        delete item;
    }
    
    ///

    void setLabelProperties(QnProxyLabel *label
        , QGraphicsLinearLayout *layout
        , Qt::Alignment align
        , bool bold
        , bool italic
        , const QColor &backgroundColor)
    {
        QFont font = label->font();
        font.setBold(bold);
        font.setItalic(italic);

        QPalette &palette = label->palette();
        palette.setColor(QPalette::Background, backgroundColor);
        label->setPalette(palette);

        label->setAlignment(align);
        label->setFont(font);

        label->setWordWrap(true);
        label->setMaximumWidth(kBookmarkFrameWidth);
        label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);

        layout->addItem(label);
    }

    void setLabelText(QnProxyLabel *label
        , const QString& text)
    {
        const QString newText = text.trimmed();

        label->setVisible(!newText.isEmpty());
        if (newText.isEmpty())
        {
            return;
        }

        label->setText(newText);
    }

    QString tagsToString(const QnCameraBookmarkTags& tags)
    {
        QString result;
        for(const auto& tag : tags)
        {
            result += (tag +  kDelimitter);
        }
        if (!result.isEmpty())
        {
            result.remove(result.length() - 1, kDelimitter.size());  /// removes last delimitter (whitespace)
        }
        return result;
    }

    ///

    typedef std::function<void (const QString &)> ButtonLabelHandlerType;
    QnProxyLabel *createButtonLabel(const QString &caption
        , const QString &id
        , QGraphicsItem *parent
        , const ButtonLabelHandlerType &handler
        , const QColor &backgroundColor)
    {
        QnProxyLabel *label = new QnProxyLabel(parent);
        label->setText(kLinkTemplate.arg(id, caption));
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        QObject::connect(label, &QnProxyLabel::linkActivated, label, [handler](const QString &id) { handler(id); });
        return label;
    }

    ///

    class BookmarkToolTipFrame : public QnToolTipWidget
    {
        typedef std::function<void (const QnCameraBookmark &)> BookmarkActionFunctionType;
    public:
        static BookmarkToolTipFrame *create(
            const BookmarkActionFunctionType &editActionFunc
            , const BookmarkActionFunctionType &removeActionFunc
            , BookmarkToolTipFrame *&firstItemRef
            , BookmarkToolTipFrame *next
            , QGraphicsItem *parent
            , HoverFocusProcessor *hoverProcessor);

        virtual ~BookmarkToolTipFrame();

        ///

        void setBookmark(const QnCameraBookmark &bookmark);

        const QnCameraBookmark &bookmark() const;
        
        void setPosition(const QPointF &pos);

        BookmarkToolTipFrame *next() const;        

    private:
        BookmarkToolTipFrame(const BookmarkActionFunctionType &editActionFunc
            , const BookmarkActionFunctionType &removeActionFunc
            , BookmarkToolTipFrame *&firstItemRef
            , QGraphicsItem *parent
            , HoverFocusProcessor *hoverProcessor);

        BookmarkToolTipFrame *prev() const;

        void setNext(BookmarkToolTipFrame *next
            , bool allowReverseSet);

        void setPrev(BookmarkToolTipFrame *prev
            , bool allowReverseSet);

        void onBookmarkAction(const QString &anchorName);

    private:
        const BookmarkActionFunctionType m_editAction;
        const BookmarkActionFunctionType m_removeAction;

        QnCameraBookmark m_bookmark;
        BookmarkToolTipFrame *&m_firstItemRef;
        BookmarkToolTipFrame *m_next;
        BookmarkToolTipFrame *m_prev;

        QGraphicsLinearLayout *m_layout;
        QnProxyLabel *m_name;
        QnProxyLabel *m_description;
        QnProxyLabel *m_tags;
    };

    BookmarkToolTipFrame *BookmarkToolTipFrame::create(
        const BookmarkActionFunctionType &editActionFunc
        , const BookmarkActionFunctionType &removeActionFunc
        , BookmarkToolTipFrame *&firstItemRef
        , BookmarkToolTipFrame *prev
        , QGraphicsItem *parent
        , HoverFocusProcessor *hoverProcessor)
    {
        BookmarkToolTipFrame *result = new BookmarkToolTipFrame(
            editActionFunc, removeActionFunc, firstItemRef, parent, hoverProcessor);
        result->setNext(nullptr, true);
        result->setPrev(prev, true);
        return result;
    }

    BookmarkToolTipFrame::BookmarkToolTipFrame(const BookmarkActionFunctionType &editActionFunc
        , const BookmarkActionFunctionType &removeActionFunc
        , BookmarkToolTipFrame *&firstItemRef
        , QGraphicsItem *parent
        , HoverFocusProcessor *hoverProcessor)
        : QnToolTipWidget(parent)
        , m_editAction(editActionFunc)
        , m_removeAction(removeActionFunc)

        , m_bookmark()
        , m_firstItemRef(firstItemRef)
        , m_next(nullptr)
        , m_prev(nullptr)

        , m_layout(new QGraphicsLinearLayout(Qt::Vertical, this))
        , m_name(new QnProxyLabel(this))
        , m_description(new QnProxyLabel(this))
        , m_tags(new QnProxyLabel(this))
    {   
        hoverProcessor->addTargetItem(this);

        setMinimumWidth(kBookmarkFrameWidth);
        setMaximumWidth(kBookmarkFrameWidth);

        const QColor backgroundColor = palette().color(QPalette::Background);
        setLabelProperties(m_name, m_layout, Qt::AlignCenter, true, false, backgroundColor);
        setLabelProperties(m_description, m_layout, Qt::AlignJustify, false, false, backgroundColor);
        setLabelProperties(m_tags, m_layout, Qt::AlignCenter, false, true, backgroundColor);

        /// TODO #ynikitenkov : Add visibility constraints according to buisiness logic

        QGraphicsLinearLayout *actionsLayout = new QGraphicsLinearLayout(Qt::Horizontal);

        QnProxyLabel *editActionLabel = createButtonLabel(kEditLabelCaption, kEditActionAnchorName, this
            , std::bind(&BookmarkToolTipFrame::onBookmarkAction, this, std::placeholders::_1), backgroundColor);
        QnProxyLabel *removeActionLabel = createButtonLabel(kRemoveLabelCaption, kRemoveActoinAnchorName, this
            , std::bind(&BookmarkToolTipFrame::onBookmarkAction, this, std::placeholders::_1), backgroundColor);

        actionsLayout->addItem(removeActionLabel);
        actionsLayout->addStretch();
        actionsLayout->addItem(editActionLabel);
        m_layout->addItem(actionsLayout);        
    }

    BookmarkToolTipFrame::~BookmarkToolTipFrame()
    {
        BookmarkToolTipFrame * const prevFrame = prev();
        BookmarkToolTipFrame * const nextFrame = next();

        if (prevFrame)
        {
            prevFrame->setNext(nextFrame, true);
        }
        
        if (nextFrame)
        {
            nextFrame->setPrev(prevFrame, true);
        }
        else if (!prevFrame)
        {
            m_firstItemRef = nullptr;
        }
    }

    BookmarkToolTipFrame *BookmarkToolTipFrame::next() const
    {
        return m_next;
    }

    BookmarkToolTipFrame *BookmarkToolTipFrame::prev() const
    {
        return m_prev;
    }

    void BookmarkToolTipFrame::setNext(BookmarkToolTipFrame *next
        , bool allowReverseSet)
    {
        m_next = next;
        if (m_next && allowReverseSet)
            m_next->setPrev(this, false);
    }

    void BookmarkToolTipFrame::setPrev(BookmarkToolTipFrame *prev
        , bool allowReverseSet)
    {
        m_prev = prev;

        if (!m_prev)
        {
            m_firstItemRef = this; /// updates pointer to first member of linked list
        }
        else if (allowReverseSet)
        {
            m_prev->setNext(this, false);
        }
    }

    void BookmarkToolTipFrame::setPosition(const QPointF &pos)
    {
        enum 
        {
            kTailLength = 55
            , kSpacerHeight = 2
            , kTailWidth = 10
        };

        const bool isFirstItem = !prev();
        const auto height = sizeHint(Qt::SizeHint::PreferredSize).height();
        const auto offset = height + (isFirstItem ? kTailLength : kSpacerHeight) ;
        const auto currentPos = pos - QPointF(0, offset);
        if (isFirstItem)
        {
            setTailPos(QPointF(kHalfBookmarkFrameWidth,  height + kTailLength));
            setTailWidth(kTailWidth);
            pointTo(pos);
        }
        else
        {
            setTailWidth(0);
            setTailPos(QPointF(100, 0));
            setPos(currentPos - QPointF(kHalfBookmarkFrameWidth, 0));
        }

        if (m_next)
        {
            const QPointF nextPosition = m_next->pos();
            m_next->setPosition(currentPos);
        }  
    }

    void BookmarkToolTipFrame::onBookmarkAction(const QString &anchorName)
    {
        const auto callActon = (anchorName == kEditActionAnchorName ? m_editAction : m_removeAction);
        callActon(m_bookmark);
    }

    void BookmarkToolTipFrame::setBookmark(const QnCameraBookmark &bookmark)
    {
        m_bookmark = bookmark;
        setLabelText(m_name, bookmark.name);
        setLabelText(m_description, bookmark.description);
        setLabelText(m_tags, tagsToString(bookmark.tags));
        m_layout->updateGeometry();
        m_layout->invalidate();
        update();
    }

    const QnCameraBookmark &BookmarkToolTipFrame::bookmark() const
    {
        return m_bookmark;
    }

    /// 

    enum
    {
        kBookmarksUpdateEventId = QEvent::User + 1
        , kBookmarkUpdatePositionEventId
        , kBookmarkActionEventId
    };

    class UpdateBokmarksEvent : public QEvent
    {
    public:

        UpdateBokmarksEvent(const QnActionParameters &params
            , const QnCameraBookmarkList &bookmarks);

        virtual ~UpdateBokmarksEvent();

        const QnActionParameters &parameters() const;

        const QnCameraBookmarkList &bookmarks() const;

    private:
        const QnActionParameters m_params;
        const QnCameraBookmarkList m_bookmarks;
    };

    UpdateBokmarksEvent::UpdateBokmarksEvent(const QnActionParameters &params
        , const QnCameraBookmarkList &bookmarks)
        : QEvent(static_cast<QEvent::Type>(kBookmarksUpdateEventId))
        , m_params(params)
        , m_bookmarks(bookmarks)
    {
    }

    UpdateBokmarksEvent::~UpdateBokmarksEvent() {}

    const QnActionParameters &UpdateBokmarksEvent::parameters() const
    {
        return m_params;
    }

    const QnCameraBookmarkList &UpdateBokmarksEvent::bookmarks() const
    {
        return m_bookmarks;
    }

    /// 

    class BookmarkActionEvent : public QEvent
    {
    public:
        BookmarkActionEvent(const QnCameraBookmark &bookmark
            , const QnActionParameters &params
            , bool edit);

        virtual ~BookmarkActionEvent();

        bool isEditEvent() const;

        const QnCameraBookmark &bookmark() const;

        const QnActionParameters &params() const;

    private:
        const QnCameraBookmark m_bookmark;
        const QnActionParameters m_params;
        const bool m_edit;
    };

    BookmarkActionEvent::BookmarkActionEvent(const QnCameraBookmark &bookmark
        , const QnActionParameters &params
        , bool edit)
        : QEvent(static_cast<QEvent::Type>(kBookmarkActionEventId))
        , m_bookmark(bookmark)
        , m_params(params)
        , m_edit(edit)
    {
    }

    BookmarkActionEvent::~BookmarkActionEvent() {}

    bool BookmarkActionEvent::isEditEvent() const
    {
        return m_edit;
    }
    
    const QnCameraBookmark &BookmarkActionEvent::bookmark() const
    {
        return m_bookmark;
    }

    const QnActionParameters &BookmarkActionEvent::params() const
    {
        return m_params;
    }

}

///

class QnBookmarksViewer::Impl : public QObject
{
public:
    Impl(QnBookmarksViewer *owner);

    virtual ~Impl();

    void updateBookmarks(QnCameraBookmarkList bookmarks
        , const QnActionParameters &params);

    void updatePosition(const QPointF &basePosition
        , bool immediately);

    void hide();
    
    void leaveTimeLine();

private:
    void updateBookmarksImpl(QnCameraBookmarkList bookmarks
        , const QnActionParameters &params);

    bool event(QEvent *event) override;

    void timerEvent(QTimerEvent *event) override;

    void emitBookmarkEvent(const QnCameraBookmark &bookmark
        , bool edit);

    void updatePositionImpl(const QPointF &pos);

private:
    QnBookmarksViewer * const m_owner;
    HoverFocusProcessor * const m_hoverProcessor;
    QnCameraBookmarkList m_bookmarks;
    BookmarkToolTipFrame *m_headFrame;

    QnActionParameters m_parameters;

    QPointF m_position;
    QPointF m_futurePosition;
    int m_positionTimerId;
};

QnBookmarksViewer::Impl::Impl(QnBookmarksViewer *owner)
    : QObject(owner)
    , m_owner(owner)
    , m_hoverProcessor(new HoverFocusProcessor(owner))
    , m_bookmarks()
    , m_headFrame(nullptr)

    , m_parameters()

    , m_position()
    , m_futurePosition()
    , m_positionTimerId(kTimerInvalidId)
{
    m_hoverProcessor->setHoverLeaveDelay(kToolTipHidingTime);
    QObject::connect(m_hoverProcessor, &HoverFocusProcessor::hoverLeft, this, &QnBookmarksViewer::Impl::hide);
}

QnBookmarksViewer::Impl::~Impl()
{
}

void QnBookmarksViewer::Impl::updateBookmarks(QnCameraBookmarkList bookmarks
    , const QnActionParameters &params)
{
    qApp->postEvent(this, new UpdateBokmarksEvent(params, bookmarks));
}

void QnBookmarksViewer::Impl::updateBookmarksImpl(QnCameraBookmarkList bookmarks
    , const QnActionParameters &params)
{
    if (bookmarks == m_bookmarks)
        return;

    m_parameters = params;
    m_bookmarks = bookmarks;

    /// removes all old frames, updates newly added
    typedef std::list<QnCameraBookmarkList::iterator> BookmarksItsContainer;

    BookmarkToolTipFrame *lastFrame = nullptr;
    for(BookmarkToolTipFrame *frame = m_headFrame; frame; )
    {
        const QnUuid frameBookmarkId = frame->bookmark().guid;
        const auto itNewBookmark = std::find_if(bookmarks.begin(), bookmarks.end()
            , [&frameBookmarkId](const QnCameraBookmark &bookmark) { return (bookmark.guid == frameBookmarkId);});

        BookmarkToolTipFrame * const next = frame->next();
        if (itNewBookmark == bookmarks.end())       /// If not found in the new list of bookmarks
        {
            deleteGraphicItem(frame);
        }
        else
        {
            frame->setBookmark(*itNewBookmark);
            bookmarks.erase(itNewBookmark);
            lastFrame = frame;
        }

        frame = next;
    }

    /// Adds new frames
    for(const auto& bookmark : bookmarks)
    {
        BookmarkToolTipFrame *newBookmarkFrame = BookmarkToolTipFrame::create(
            [this](QnCameraBookmark bookmark) { emitBookmarkEvent(bookmark, true); }
            , [this](QnCameraBookmark bookmark) { emitBookmarkEvent(bookmark, false); }
            , m_headFrame, lastFrame, m_owner, m_hoverProcessor);
        newBookmarkFrame->setBookmark(bookmark);
        lastFrame = newBookmarkFrame;
     }

    updatePositionImpl(m_futurePosition);
}

bool QnBookmarksViewer::Impl::event(QEvent *event)
{
    switch(event->type())
    {
    case kBookmarksUpdateEventId: 
    {
        auto const updateEvent = static_cast<UpdateBokmarksEvent *>(event);
        updateBookmarksImpl(updateEvent->bookmarks(), updateEvent->parameters());
        break;
    }
    case kBookmarkActionEventId:
    {
        auto const bookmarkActionEvent = static_cast<BookmarkActionEvent *>(event);
        const auto& bookmark = bookmarkActionEvent->bookmark();
        const auto& params = bookmarkActionEvent->params();
        if (bookmarkActionEvent->isEditEvent())
        {
            emit m_owner->editBookmarkClicked(bookmark, params);
        }
        else
        {
            emit m_owner->removeBookmarkClicked(bookmark, params);
        }
        break;
    }
    case kBookmarkUpdatePositionEventId:
        if (!m_bookmarks.empty())
        {
            updatePositionImpl(m_futurePosition);
        }
        break;
    default:
        return QObject::event(event);
    }

    return true;
}

void QnBookmarksViewer::Impl::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_positionTimerId)
    {
        killTimer(m_positionTimerId);
        m_positionTimerId = kTimerInvalidId;
        updatePositionImpl(m_futurePosition);
    }
}

void QnBookmarksViewer::Impl::emitBookmarkEvent(const QnCameraBookmark &bookmark
    , bool edit)
{
    hide();
    qApp->postEvent(this, new BookmarkActionEvent(bookmark, m_parameters, edit));
}

void QnBookmarksViewer::Impl::updatePosition(const QPointF &basePosition
    , bool immediately)
{
    m_hoverProcessor->forceHoverEnter();

    m_futurePosition = basePosition;
    if (m_positionTimerId != kTimerInvalidId)
    {
        killTimer(m_positionTimerId);
        m_positionTimerId = kTimerInvalidId;
    }
    
    if (immediately || (m_positionTimerId = startTimer(kTimerPeriod)) == kTimerInvalidId)
    {
        updatePositionImpl(basePosition);
    }
}

void QnBookmarksViewer::Impl::updatePositionImpl(const QPointF &pos)
{
    m_position = pos;

    if (m_headFrame)
    {
        m_headFrame->setPosition(pos);
    }
}


void QnBookmarksViewer::Impl::hide()
{
    updateBookmarks(QnCameraBookmarkList(), m_parameters);
}

void QnBookmarksViewer::Impl::leaveTimeLine()
{
    m_hoverProcessor->forceHoverLeave();
    m_futurePosition = m_position;
}

///

QnBookmarksViewer *QnBookmarksViewer::create(QGraphicsItem *parent)
{
    return (new QnBookmarksViewer(parent));
}

QnBookmarksViewer::QnBookmarksViewer(QGraphicsItem *parent)
    : QGraphicsWidget(parent)
    , m_impl(new Impl(this))
{
}
    
QnBookmarksViewer::~QnBookmarksViewer()
{
}

void QnBookmarksViewer::updateBookmarks(const QnCameraBookmarkList &bookmarks
    , const QnActionParameters &params)
{
    m_impl->updateBookmarks(bookmarks, params);
}

void QnBookmarksViewer::updatePosition(const QPointF &basePosition
    , bool immediately)
{
    m_impl->updatePosition(
        basePosition
        , immediately);
}

void QnBookmarksViewer::hide()
{
    m_impl->hide();
}

void QnBookmarksViewer::leaveTimeLine()
{
    m_impl->leaveTimeLine();
}
