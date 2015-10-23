
#include "bookmarks_viewer.h"

#include <core/resource/camera_bookmark.h>

#include <ui/common/palette.h>
#include <ui/processors/hover_processor.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/actions/action_parameters.h>

namespace
{
    typedef std::shared_ptr<HoverFocusProcessor> HoverFocusProcessorPointer;

    const auto kBookmarkColor = QColor("#204969");

    enum { kToolTipHidingTime = 3000 };

    enum 
    {
        kBookmarkFrameWidth = 250
        , kHalfBookmarkFrameWidth = kBookmarkFrameWidth / 2
    };

    enum LabelParamIds
    {
        kNameLabelIndex
        , kDescriptionLabelIndex
    };

    struct LabelParams
    {
        Qt::Alignment align;
        bool bold;
        int fontSize;

        LabelParams(Qt::Alignment initAlign
            , bool initBold
            , int initFontSize);
    };

    LabelParams::LabelParams(Qt::Alignment initAlign
        , bool initBold
        , int initFontSize)
        : align(initAlign)
        , bold(initBold)
        , fontSize(initFontSize)
    {}

    /// Array of parameters for label. Indexed by LabelParamIds enum
    const LabelParams kLabelParams[] = 
    {
        LabelParams(Qt::AlignLeft, true, 16)        /// For name label
        , LabelParams(Qt::AlignLeft, false, 12)     /// For description label
    };

    const QString kEditActionAnchorName = lit("e");
    const QString kRemoveActoinAnchorName = lit("r");
    const QString kLinkTemplate = lit("<a href = \"%1\">%2</a>");

    class QnBookmarksViewerStrings
    {
        Q_DECLARE_TR_FUNCTIONS(QnBookmarkTooltipActionResuourceStrings)
    public:
        static const QString editCaption() { return tr("Edit"); }
        static const QString removeCaption() { return tr("Remove"); }
    };

    /// 

    void deleteGraphicItem(QGraphicsItem *item)
    {
        item->setParentItem(nullptr);
        delete item;
    }
    
    ///

    /// @brief Updates label text. If text is empty it removes label, otherwise tries to recreate it
    int renewLabel(QnProxyLabel *&label
        , const QString &text
        , QGraphicsItem *parent
        , QGraphicsLinearLayout *layout
        , int insertionIndex
        , LabelParamIds labelParamsId)
    {
        const QString trimmedText = text.trimmed();

        if (text.isEmpty())
        {
            if (label)
            {
                layout->removeItem(label);
                delete label;
                label = nullptr;
            }
            return insertionIndex;
        }
        
        if (!label)
        {
            label = new QnProxyLabel(parent);
            layout->insertItem(insertionIndex, label);

            const LabelParams &params = kLabelParams[labelParamsId];
            QFont font = label->font();
            font.setBold(params.bold);
            font.setPixelSize(params.fontSize);
            label->setFont(font);

            setPaletteColor(label, QPalette::Background, Qt::transparent);

            label->setWordWrap(true);
            
            label->setMaximumWidth(kBookmarkFrameWidth - label->geometry().x() * 2);
            label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);
            label->setAlignment(params.align);
        }

        static const QString _t = lit("<p style = \"word-wrap:break-word; white-space:pre-wrap; color:red\">%1</p>");
        label->setText(_t.arg(trimmedText));
        return (insertionIndex + 1);
    }

    ///

    typedef std::function<void (const QString &)> ButtonLabelHandlerType;
    QnProxyLabel *createButtonLabel(const QString &caption
        , const QString &id
        , QGraphicsItem *parent
        , const ButtonLabelHandlerType &handler)
    {
        QnProxyLabel *label = new QnProxyLabel(parent);
        label->setText(kLinkTemplate.arg(id, caption));
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        QObject::connect(label, &QnProxyLabel::linkActivated, label, [handler](const QString &id) { handler(id); });
        
        setPaletteColor(label, QPalette::Background, Qt::transparent);

        return label;
    }

    ///

    class ProcessorHolder : private boost::noncopyable
    {
    public:
        ProcessorHolder(const HoverFocusProcessorPointer &processor
            , QGraphicsItem *item);

        ~ProcessorHolder();

    private:
        const HoverFocusProcessorPointer m_processor;
        QGraphicsItem * const m_item;
    };

    ProcessorHolder::ProcessorHolder(const HoverFocusProcessorPointer &processor
        , QGraphicsItem *item)
        : m_processor(processor)
        , m_item(item)
    {
        processor->addTargetItem(item);
    }

    ProcessorHolder::~ProcessorHolder()
    {
        m_processor->removeTargetItem(m_item);
    }

    ///

    class BookmarkToolTipFrame : public QnToolTipWidget
    {
        typedef std::function<void (const QnCameraBookmark &)> BookmarkActionFunctionType;
    public:
        static BookmarkToolTipFrame *create(
            const BookmarkActionFunctionType &editActionFunc
            , const BookmarkActionFunctionType &removeActionFunc
            , const HoverFocusProcessorPointer &hoverProcessor
            , BookmarkToolTipFrame *&firstItemRef
            , BookmarkToolTipFrame *next
            , QGraphicsItem *parent);

        virtual ~BookmarkToolTipFrame();

        ///

        void setBookmark(const QnCameraBookmark &bookmark);

        const QnCameraBookmark &bookmark() const;
        
        void setPosition(const QPointF &pos);

        BookmarkToolTipFrame *next() const;        

    private:
        BookmarkToolTipFrame(const BookmarkActionFunctionType &editActionFunc
            , const BookmarkActionFunctionType &removeActionFunc
            , const HoverFocusProcessorPointer &hoverProcessor
            , BookmarkToolTipFrame *&firstItemRef
            , QGraphicsItem *parent);

        BookmarkToolTipFrame *prev() const;

        void setNext(BookmarkToolTipFrame *next
            , bool allowReverseSet);

        void setPrev(BookmarkToolTipFrame *prev
            , bool allowReverseSet);

        void onBookmarkAction(const QString &anchorName);

        
        virtual void setGeometry(const QRectF &rect) override
        {
            QnToolTipWidget::setGeometry(rect);
        }

    private:
        const BookmarkActionFunctionType m_editAction;
        const BookmarkActionFunctionType m_removeAction;

        ProcessorHolder m_processorHolder;

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
        , const HoverFocusProcessorPointer &hoverProcessor
        , BookmarkToolTipFrame *&firstItemRef
        , BookmarkToolTipFrame *prev
        , QGraphicsItem *parent)
    {
        BookmarkToolTipFrame *result = new BookmarkToolTipFrame(
            editActionFunc, removeActionFunc, hoverProcessor, firstItemRef, parent);
        result->setNext(nullptr, true);
        result->setPrev(prev, true);
        return result;
    }

    BookmarkToolTipFrame::BookmarkToolTipFrame(const BookmarkActionFunctionType &editActionFunc
        , const BookmarkActionFunctionType &removeActionFunc
        , const HoverFocusProcessorPointer &hoverProcessor
        , BookmarkToolTipFrame *&firstItemRef
        , QGraphicsItem *parent)
        : QnToolTipWidget(parent)
        , m_editAction(editActionFunc)
        , m_removeAction(removeActionFunc)

        , m_processorHolder(hoverProcessor, this)

        , m_bookmark()
        , m_firstItemRef(firstItemRef)
        , m_next(nullptr)
        , m_prev(nullptr)

        , m_layout(new QGraphicsLinearLayout(Qt::Vertical, this))
        , m_name(nullptr)
        , m_description(nullptr)
        , m_tags(nullptr)
    {   
        setMinimumWidth(kBookmarkFrameWidth);
        setMaximumWidth(kBookmarkFrameWidth);

        setWindowColor(kBookmarkColor);
        setFrameColor(kBookmarkColor);

        /// TODO #ynikitenkov : Add visibility constraints according to buisiness logic

        QGraphicsLinearLayout *actionsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
        QnProxyLabel *editActionLabel =
            createButtonLabel(QnBookmarksViewerStrings::editCaption(), kEditActionAnchorName, this
            , std::bind(&BookmarkToolTipFrame::onBookmarkAction, this, std::placeholders::_1));
        QnProxyLabel *removeActionLabel =
            createButtonLabel(QnBookmarksViewerStrings::removeCaption(), kRemoveActoinAnchorName, this
            , std::bind(&BookmarkToolTipFrame::onBookmarkAction, this, std::placeholders::_1));

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
            prevFrame->setNext(nextFrame, true);
        
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
            kTailHeight = 10
            , kTailWidth = 20
            , kHalfTailWidth = kTailWidth / 2
            , kTailOffset = 15
            , kSpacerHeight = 3
        };

        setTailWidth(kTailWidth);

        const auto b = boundingRect();
        const auto cm = contentsMargins();
        const auto cr = contentsRect();

        const bool isFirstItem = !prev();
        
        updateGeometry();

        const auto height = geometry().height();
        const auto offset = height + (isFirstItem ? kTailHeight : kSpacerHeight);
        const auto currentPos = pos - QPointF(0, offset);

        const auto tailHorOffset = kTailOffset + kHalfTailWidth;
        if (isFirstItem)
        {
            setTailPos(QPointF(tailHorOffset, offset));
            pointTo(pos);
        }
        else
        {
            setPos(currentPos - QPointF(tailHorOffset, 0));
        }

        if (m_next)
            m_next->setPosition(currentPos);
    }

    void BookmarkToolTipFrame::onBookmarkAction(const QString &anchorName)
    {
        const auto callActon = (anchorName == kEditActionAnchorName ? m_editAction : m_removeAction);
        callActon(m_bookmark);
    }

    void BookmarkToolTipFrame::setBookmark(const QnCameraBookmark &bookmark)
    {
        m_bookmark = bookmark;

        enum { kFirstPosition = 0 };
        const int position = renewLabel(m_name, bookmark.name, this, m_layout, kFirstPosition, kNameLabelIndex);
        renewLabel(m_description, bookmark.description, this, m_layout, position, kDescriptionLabelIndex);
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
        , kBookmarkEditActionEventId
        , kBookmarkRemoveActionEventId
    };

    ///

    class UpdateBokmarksEvent : public QEvent
    {
    public:

        UpdateBokmarksEvent(const QnCameraBookmarkList &bookmarks);

        virtual ~UpdateBokmarksEvent();

        const QnCameraBookmarkList &bookmarks() const;

    private:
        const QnCameraBookmarkList m_bookmarks;
    };

    UpdateBokmarksEvent::UpdateBokmarksEvent(const QnCameraBookmarkList &bookmarks)
        : QEvent(static_cast<QEvent::Type>(kBookmarksUpdateEventId))
        , m_bookmarks(bookmarks)
    {
    }

    UpdateBokmarksEvent::~UpdateBokmarksEvent() {}

    const QnCameraBookmarkList &UpdateBokmarksEvent::bookmarks() const
    {
        return m_bookmarks;
    }

    /// 

    /// @class BookmarkActionEvent
    /// @brief Stores paramteres for bookmark action generation
    class BookmarkActionEvent : public QEvent
    {
    public:
        BookmarkActionEvent(int eventId
            , const QnCameraBookmark &bookmark);

        virtual ~BookmarkActionEvent();

        const QnCameraBookmark &bookmark() const;

    private:
        const QnCameraBookmark m_bookmark;
    };

    BookmarkActionEvent::BookmarkActionEvent(int eventId
        , const QnCameraBookmark &bookmark)
        : QEvent(static_cast<QEvent::Type>(eventId))
        , m_bookmark(bookmark)
    {
    }

    BookmarkActionEvent::~BookmarkActionEvent() {}

    const QnCameraBookmark &BookmarkActionEvent::bookmark() const
    {
        return m_bookmark;
    }
}

///

class QnBookmarksViewer::Impl : public QObject
{
public:
    Impl(const GetBookmarksFunc &getBookmarksFunc
        , const GetPosOnTimelineFunc &getPosFunc
        , QnBookmarksViewer *owner);

    virtual ~Impl();

    ///

    void setTargetTimestamp(qint64 timestamp);

    void updateOnWindowChange();

    ///

    void resetBookmarks();

    ///

    bool updateBookmarks(QnCameraBookmarkList bookmarks);

    void updatePosition(const QPointF &basePosition);

    void hide();
    
    void hideDelayed();

private:
    void updateBookmarksImpl(QnCameraBookmarkList bookmarks);

    bool event(QEvent *event) override;

    void emitBookmarkEvent(const QnCameraBookmark &bookmark
        , int eventId);

    void updatePositionImpl(const QPointF &pos);

private:
    const GetBookmarksFunc m_getBookmarks;
    const GetPosOnTimelineFunc m_getPos;

    qint64 m_targetTimestamp;

    const HoverFocusProcessorPointer m_hoverProcessor;
    QnBookmarksViewer * const m_owner;
    QnCameraBookmarkList m_bookmarks;
    BookmarkToolTipFrame *m_headFrame;

    QPointF m_position;
    QPointF m_futurePosition;
};

enum { kInvalidTimstamp = -1 };

QnBookmarksViewer::Impl::Impl(const GetBookmarksFunc &getBookmarksFunc
    , const GetPosOnTimelineFunc &getPosFunc
    , QnBookmarksViewer *owner)

    : QObject(owner)
    
    , m_getBookmarks(getBookmarksFunc)
    , m_getPos(getPosFunc)

    , m_targetTimestamp(kInvalidTimstamp)

    , m_hoverProcessor(new HoverFocusProcessor())
    , m_owner(owner)
    , m_bookmarks()
    , m_headFrame(nullptr)

    , m_position()
    , m_futurePosition()
{
//    m_hoverProcessor->setHoverLeaveDelay(kToolTipHidingTime);
//    QObject::connect(m_hoverProcessor.get(), &HoverFocusProcessor::hoverLeft, this, &QnBookmarksViewer::Impl::hide);
}

QnBookmarksViewer::Impl::~Impl()
{
}

void QnBookmarksViewer::Impl::setTargetTimestamp(qint64 timestamp)
{
    if (m_targetTimestamp == timestamp)
        return;

    const auto newBookmarks = m_getBookmarks(timestamp);
    if (m_bookmarks == newBookmarks)
        return;

    if (newBookmarks.empty())
    {
        resetBookmarks();
        return;
    }

    m_owner->setVisible(false);

    m_targetTimestamp = timestamp;
    updateBookmarksImpl(newBookmarks);
    updatePosition(m_getPos(m_targetTimestamp));
}

void QnBookmarksViewer::Impl::updateOnWindowChange()
{
    if (m_targetTimestamp == kInvalidTimstamp)
        return;

    const auto newPos = m_getPos(m_targetTimestamp);
    if (newPos.isNull())
    {
        updateBookmarksImpl(QnCameraBookmarkList());
        return;
    }

    if (m_bookmarks.empty())
    {
        m_owner->setVisible(false);
        updateBookmarksImpl(m_getBookmarks(m_targetTimestamp));
        updatePosition(newPos);
    }
    else
        updatePositionImpl(newPos);
}

void QnBookmarksViewer::Impl::resetBookmarks()
{
    if (m_targetTimestamp == kInvalidTimstamp)
        return;

    m_targetTimestamp = kInvalidTimstamp;
    updateBookmarksImpl(QnCameraBookmarkList());
}


bool QnBookmarksViewer::Impl::updateBookmarks(QnCameraBookmarkList bookmarks)
{
    qApp->postEvent(this, new UpdateBokmarksEvent(bookmarks));
    return (bookmarks != m_bookmarks);
}

void QnBookmarksViewer::Impl::updateBookmarksImpl(QnCameraBookmarkList bookmarks)
{
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
        lastFrame = BookmarkToolTipFrame::create(
            [this](const QnCameraBookmark &bookmark) { emitBookmarkEvent(bookmark, kBookmarkEditActionEventId); }
            , [this](const QnCameraBookmark &bookmark) { emitBookmarkEvent(bookmark, kBookmarkRemoveActionEventId); }
            , m_hoverProcessor, m_headFrame, lastFrame, m_owner);
        lastFrame->setBookmark(bookmark);
     }
}

bool QnBookmarksViewer::Impl::event(QEvent *event)
{
    switch(event->type())
    {
    case kBookmarksUpdateEventId: 
    {
        const auto updateEvent = static_cast<UpdateBokmarksEvent *>(event);
        updateBookmarksImpl(updateEvent->bookmarks());
        break;
    }
    case kBookmarkEditActionEventId:
    {
        const auto bookmarkActionEvent = static_cast<BookmarkActionEvent *>(event);
        emit m_owner->editBookmarkClicked(bookmarkActionEvent->bookmark());
        break;
    }
    case kBookmarkRemoveActionEventId:
    {
        const auto bookmarkActionEvent = static_cast<BookmarkActionEvent *>(event);
        emit m_owner->removeBookmarkClicked(bookmarkActionEvent->bookmark());
        break;
    }
    case kBookmarkUpdatePositionEventId:
    {
        if (!m_bookmarks.empty())
            updatePositionImpl(m_futurePosition);

        m_owner->setVisible(true);
        break;
    }
    default:
        return QObject::event(event);
    }

    return true;
}

void QnBookmarksViewer::Impl::emitBookmarkEvent(const QnCameraBookmark &bookmark
    , int eventId)
{
    hide();
    qApp->postEvent(this, new BookmarkActionEvent(eventId, bookmark));
}

void QnBookmarksViewer::Impl::updatePosition(const QPointF &basePosition)
{
    m_futurePosition = basePosition;
    qApp->postEvent(this, new QEvent(static_cast<QEvent::Type>(kBookmarkUpdatePositionEventId)));
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
    updateBookmarks(QnCameraBookmarkList());
}

void QnBookmarksViewer::Impl::hideDelayed()
{
    m_hoverProcessor->forceHoverLeave();
    m_futurePosition = m_position;
}

///

QnBookmarksViewer::QnBookmarksViewer(const GetBookmarksFunc &getBookmarksFunc
    , const GetPosOnTimelineFunc &getPosFunc
    , QGraphicsItem *parent)
    : QGraphicsWidget(parent)
    , m_impl(new Impl(getBookmarksFunc, getPosFunc, this))
{
}
    
QnBookmarksViewer::~QnBookmarksViewer()
{
}

void QnBookmarksViewer::setTargetTimestamp(qint64 timestamp)
{
    m_impl->setTargetTimestamp(timestamp);
}

void QnBookmarksViewer::updateOnWindowChange()
{
    m_impl->updateOnWindowChange();
}

void QnBookmarksViewer::resetBookmarks()
{
    m_impl->resetBookmarks();
}
