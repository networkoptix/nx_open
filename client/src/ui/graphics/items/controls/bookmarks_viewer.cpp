
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

    enum { kToolTipHidingTime = 3000 };

    enum 
    {
        kTimerInvalidId = 0
        , kTimerPeriod = 700
    };

    enum 
    {
        kBookmarkFrameWidth = 250
        , kHalfBookmarkFrameWidth = kBookmarkFrameWidth / 2
    };

    enum LabelParamIds
    {
        kNameLabelIndex
        , kDescriptionLabelIndex
        , kTagsLabelIndex
    };

    struct LabelParams
    {
        Qt::Alignment align;
        bool bold;
        bool italic;
    };

    /// Array of parameters for label. Indexed by LabelParamIds enum
    const LabelParams kLabelParams[] = 
    {
        {Qt::AlignCenter, true, true}           /// For name label
        , {Qt::AlignLeft, false, false}         /// For description label
        , {Qt::AlignCenter, false, true}        /// For tags label
    };

    const QString kEditActionAnchorName = lit("e");
    const QString kRemoveActoinAnchorName = lit("r");
    const QString kDelimiter = lit(" ");
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
        , LabelParamIds labelParamsId
        , const QColor &backgroundColor)
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
            font.setItalic(params.italic);
            label->setFont(font);

            setPaletteColor(label, QPalette::Background, backgroundColor);

            label->setWordWrap(true);
            label->setMaximumWidth(kBookmarkFrameWidth);
            label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);
            label->setAlignment(params.align);
        }

        label->setText(trimmedText);
        return (insertionIndex + 1);
    }

    QString tagsToString(const QnCameraBookmarkTags& tags)
    {
        QString result;
        for(const auto& tag : tags)
        {
            result += (tag +  kDelimiter);
        }
        if (!result.isEmpty())
        {
            result.remove(result.length() - 1, kDelimiter.size());  /// removes last delimiter (whitespace)
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
        
        setPaletteColor(label, QPalette::Background, backgroundColor);

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

        /// TODO #ynikitenkov : Add visibility constraints according to buisiness logic

        const QColor backgroundColor = palette().color(QPalette::Background);
        QGraphicsLinearLayout *actionsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
        QnProxyLabel *editActionLabel =
            createButtonLabel(QnBookmarksViewerStrings::editCaption(), kEditActionAnchorName, this
            , std::bind(&BookmarkToolTipFrame::onBookmarkAction, this, std::placeholders::_1), backgroundColor);
        QnProxyLabel *removeActionLabel =
            createButtonLabel(QnBookmarksViewerStrings::removeCaption(), kRemoveActoinAnchorName, this
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
            kTailLength = 55
            , kSpacerHeight = 3
            , kTailWidth = 10
        };

        setTailWidth(kTailWidth);

        const bool isFirstItem = !prev();
        const auto height = sizeHint(Qt::SizeHint::PreferredSize).height();
        const auto offset = height + (isFirstItem ? kTailLength : kSpacerHeight);
        const auto currentPos = pos - QPointF(0, offset);

        if (isFirstItem)
        {
            setTailPos(QPointF(kHalfBookmarkFrameWidth, offset));
            pointTo(pos);
        }
        else
        {
            setPos(currentPos - QPointF(kHalfBookmarkFrameWidth, 0));
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
        const QColor backgroundColor = palette().color(QPalette::Background);
        int position = renewLabel(m_name, bookmark.name, this, m_layout, kFirstPosition, kNameLabelIndex, backgroundColor);
        position = renewLabel(m_description, bookmark.description, this, m_layout, position, kDescriptionLabelIndex, backgroundColor);
        position = renewLabel(m_tags, tagsToString(bookmark.tags), this, m_layout, position, kTagsLabelIndex, backgroundColor);
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

    /// @class BookmarkActionEvent
    /// @brief Stores paramteres for bookmark action generation
    class BookmarkActionEvent : public QEvent
    {
    public:
        BookmarkActionEvent(int eventId
            , const QnCameraBookmark &bookmark
            , const QnActionParameters &params);

        virtual ~BookmarkActionEvent();

        const QnCameraBookmark &bookmark() const;

        const QnActionParameters &params() const;

    private:
        const QnCameraBookmark m_bookmark;
        const QnActionParameters m_params;
    };

    BookmarkActionEvent::BookmarkActionEvent(int eventId
        , const QnCameraBookmark &bookmark
        , const QnActionParameters &params)
        : QEvent(static_cast<QEvent::Type>(eventId))
        , m_bookmark(bookmark)
        , m_params(params)
    {
    }

    BookmarkActionEvent::~BookmarkActionEvent() {}

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
    
    void hideDelayed();

    void resetTimer();

private:
    void updateBookmarksImpl(QnCameraBookmarkList bookmarks
        , const QnActionParameters &params);

    bool event(QEvent *event) override;

    void timerEvent(QTimerEvent *event) override;

    void emitBookmarkEvent(const QnCameraBookmark &bookmark
        , int eventId);

    void updatePositionImpl(const QPointF &pos);

private:
    const HoverFocusProcessorPointer m_hoverProcessor;
    QnBookmarksViewer * const m_owner;
    QnCameraBookmarkList m_bookmarks;
    BookmarkToolTipFrame *m_headFrame;

    QnActionParameters m_parameters;

    QPointF m_position;
    QPointF m_futurePosition;
    int m_positionTimerId;
};

QnBookmarksViewer::Impl::Impl(QnBookmarksViewer *owner)
    : QObject(owner)
    , m_hoverProcessor(new HoverFocusProcessor())
    , m_owner(owner)
    , m_bookmarks()
    , m_headFrame(nullptr)

    , m_parameters()

    , m_position()
    , m_futurePosition()
    , m_positionTimerId(kTimerInvalidId)
{
    m_hoverProcessor->setHoverLeaveDelay(kToolTipHidingTime);
    QObject::connect(m_hoverProcessor.get(), &HoverFocusProcessor::hoverLeft, this, &QnBookmarksViewer::Impl::hide);
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
        lastFrame = BookmarkToolTipFrame::create(
            [this](const QnCameraBookmark &bookmark) { emitBookmarkEvent(bookmark, kBookmarkEditActionEventId); }
            , [this](const QnCameraBookmark &bookmark) { emitBookmarkEvent(bookmark, kBookmarkRemoveActionEventId); }
            , m_hoverProcessor, m_headFrame, lastFrame, m_owner);
        lastFrame->setBookmark(bookmark);
     }

    updatePositionImpl(m_futurePosition);
}

bool QnBookmarksViewer::Impl::event(QEvent *event)
{
    switch(event->type())
    {
    case kBookmarksUpdateEventId: 
    {
        const auto const updateEvent = static_cast<UpdateBokmarksEvent *>(event);
        updateBookmarksImpl(updateEvent->bookmarks(), updateEvent->parameters());
        break;
    }
    case kBookmarkEditActionEventId:
    {
        const auto const bookmarkActionEvent = static_cast<BookmarkActionEvent *>(event);
        emit m_owner->editBookmarkClicked(bookmarkActionEvent->bookmark(), bookmarkActionEvent->params());
        break;
    }
    case kBookmarkRemoveActionEventId:
    {
        const auto const bookmarkActionEvent = static_cast<BookmarkActionEvent *>(event);
        emit m_owner->removeBookmarkClicked(bookmarkActionEvent->bookmark(), bookmarkActionEvent->params());
        break;
    }
    case kBookmarkUpdatePositionEventId:
    {
        if (!m_bookmarks.empty())
        {
            updatePositionImpl(m_futurePosition);
        }
        break;
    }
    default:
        return QObject::event(event);
    }

    return true;
}

void QnBookmarksViewer::Impl::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_positionTimerId)
    {
        resetTimer();
        updatePositionImpl(m_futurePosition);
    }
}

void QnBookmarksViewer::Impl::emitBookmarkEvent(const QnCameraBookmark &bookmark
    , int eventId)
{
    hide();
    qApp->postEvent(this, new BookmarkActionEvent(eventId, bookmark, m_parameters));
}

void QnBookmarksViewer::Impl::updatePosition(const QPointF &basePosition
    , bool immediately)
{
    m_hoverProcessor->forceHoverEnter();

    m_futurePosition = basePosition;
    resetTimer();
    
    if (immediately || ((m_positionTimerId = startTimer(kTimerPeriod)) == kTimerInvalidId))
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

void QnBookmarksViewer::Impl::hideDelayed()
{
    m_hoverProcessor->forceHoverLeave();
    m_futurePosition = m_position;
}

void QnBookmarksViewer::Impl::resetTimer()
{
    if (m_positionTimerId != kTimerInvalidId)
    {
        killTimer(m_positionTimerId);
        m_positionTimerId = kTimerInvalidId;
    }
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

void QnBookmarksViewer::hideDelayed()
{
    m_impl->hideDelayed();
}
