
#include "bookmarks_viewer.h"

#include <core/resource/camera_bookmark.h>

#include <ui/common/palette.h>
#include <ui/processors/hover_processor.h>
#include <ui/graphics/items/generic/separator.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/actions/action_parameters.h>

#include <utils/common/string.h>

#include <QTextDocument>

namespace
{
    enum 
    {
        kBookmarkFrameWidth = 250
        , kHalfBookmarkFrameWidth = kBookmarkFrameWidth / 2
    };

    enum LabelParamIds
    {
        kNameLabelIndex
        , kDescriptionLabelIndex
        , kTagsIndex
    };

    struct LabelParams
    {
        bool bold;
        int fontSize;
        int topPadding;

        LabelParams(bool initBold
            , int initFontSize
            , int initTopPadding);
    };

    LabelParams::LabelParams(bool initBold
        , int initFontSize
        , int initTopPadding)
        : bold(initBold)
        , fontSize(initFontSize)
        , topPadding(initTopPadding)
    {}

    /// Array of parameters for label. Indexed by LabelParamIds enum
    const LabelParams kLabelParams[] = 
    {
        LabelParams(true, 16, 2)        /// For name label
        , LabelParams(false, 12, 2)     /// For description label
        , LabelParams(false, 12, 4)     /// For tags label
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

    template<typename WidgetType>
    void removeWidget(WidgetType *&widget
        , QGraphicsLinearLayout *layout)
    {
        if (!widget || !layout)
            return;

        layout->removeItem(widget);
        delete widget;

        widget = nullptr;
    }

    /// @brief Updates label text. If text is empty it removes label, otherwise tries to recreate it
    int renewLabel(int insertionIndex
        
        , QnProxyLabel *&label
        , const QString &text
        
        , QGraphicsItem *parent
        , QGraphicsLinearLayout *layout

        , LabelParamIds labelParamsId)
    {
        const QString trimmedText = text.trimmed();

        if (text.isEmpty())
        {
            removeWidget(label, layout);
            return insertionIndex;
        }
        
        const bool noLabel = !label;
        const LabelParams &params = kLabelParams[labelParamsId];
        if (noLabel)
        {
            label = new QnProxyLabel(parent);
            label->setIndent(0);
            label->setMargin(0);
            layout->insertItem(insertionIndex, label);
            if (insertionIndex > 0 && params.topPadding)
                layout->setItemSpacing(insertionIndex - 1, params.topPadding);
        }

        label->setText(trimmedText);

        if (noLabel)
        {
            QFont font = label->font();
            font.setBold(params.bold);
            font.setPixelSize(params.fontSize);
            label->setFont(font);

            setPaletteColor(label, QPalette::Background, Qt::transparent);

            const auto labelSize = kBookmarkFrameWidth - label->geometry().x() * 2;

            label->setWordWrap(true);
            label->setPreferredWidth(labelSize);
            label->setAlignment(Qt::AlignLeft);
            label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
        }

        if (label->textFormat() == Qt::RichText)    /// Workaround for wrong sizeHint when rendering 'complex' html
        {
            QTextDocument td;
            td.setHtml(trimmedText);
            td.setTextWidth(label->minimumWidth());
            td.setDocumentMargin(0);

            label->setMaximumHeight(td.documentLayout()->documentSize().height());
            label->setMinimumHeight(td.documentLayout()->documentSize().height());
        }

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
        label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        QObject::connect(label, &QnProxyLabel::linkActivated, label, [handler](const QString &id) { handler(id); });
        
        setPaletteColor(label, QPalette::Background, Qt::transparent);

        return label;
    }

    ///

    class ProcessorHolder : private boost::noncopyable
    {
    public:
        ProcessorHolder(HoverFocusProcessor *processor
            , QGraphicsItem *item);

        ~ProcessorHolder();

    private:
        HoverFocusProcessor * const m_processor;
        QGraphicsItem * const m_item;
    };

    ProcessorHolder::ProcessorHolder(HoverFocusProcessor *processor
        , QGraphicsItem *item)
        : m_processor(processor)
        , m_item(item)
    {
        if (m_processor)
            m_processor->addTargetItem(item);
    }

    ProcessorHolder::~ProcessorHolder()
    {
        if (m_processor)
            m_processor->removeTargetItem(m_item);
    }

    ///

    class BookmarkToolTipFrame : public QnToolTipWidget
    {
        typedef std::function<void (const QnCameraBookmark &)> BookmarkActionFunctionType;
    public:
        static BookmarkToolTipFrame *create(
            const QnBookmarkColors &colors
            , const BookmarkActionFunctionType &editActionFunc
            , const BookmarkActionFunctionType &removeActionFunc
            , HoverFocusProcessor *hoverProcessor
            , BookmarkToolTipFrame *&firstItemRef
            , BookmarkToolTipFrame *next
            , QGraphicsItem *parent);

        virtual ~BookmarkToolTipFrame();

        ///

        void updateBookmark(const QnCameraBookmark &bookmark
            , const QnBookmarkColors &colors);

        const QnCameraBookmark &bookmark() const;
        
        void setPosition(const QnBookmarksViewer::PosAndBoundsPair &params);

        BookmarkToolTipFrame *next() const;        

    private:
        BookmarkToolTipFrame(
            const QnBookmarkColors &colors
            , const BookmarkActionFunctionType &editActionFunc
            , const BookmarkActionFunctionType &removeActionFunc
            , HoverFocusProcessor *hoverProcessor
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
        QnSeparator * const m_buttonsSeparator;
    };

    BookmarkToolTipFrame *BookmarkToolTipFrame::create(
        const QnBookmarkColors &colors
        , const BookmarkActionFunctionType &editActionFunc
        , const BookmarkActionFunctionType &removeActionFunc
        , HoverFocusProcessor *hoverProcessor
        , BookmarkToolTipFrame *&firstItemRef
        , BookmarkToolTipFrame *prev
        , QGraphicsItem *parent)
    {
        BookmarkToolTipFrame *result = new BookmarkToolTipFrame(colors, 
            editActionFunc, removeActionFunc, hoverProcessor, firstItemRef, parent);
        result->setNext(nullptr, true);
        result->setPrev(prev, true);
        return result;
    }

    BookmarkToolTipFrame::BookmarkToolTipFrame(
        const QnBookmarkColors &colors
        , const BookmarkActionFunctionType &editActionFunc
        , const BookmarkActionFunctionType &removeActionFunc
        , HoverFocusProcessor *hoverProcessor
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
        , m_buttonsSeparator(new QnSeparator(colors.separator, this))
    {   
        setPreferredWidth(kBookmarkFrameWidth);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);

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
        
        enum 
        {
            kBorderRadius = 2
            , kBaseMargin = 14
            , kHorizontalMargins = kBaseMargin - kBorderRadius
            , kTopMargin = 9
            , kBottomMargin = 7
            , kSeparatorMargin = kBottomMargin
        };

        setContentsMargins(kBorderRadius, kBorderRadius, kBorderRadius, kBorderRadius);

        m_layout->setSpacing(0);
        m_layout->setContentsMargins(kHorizontalMargins, kTopMargin
            , kHorizontalMargins, kBottomMargin);

        m_layout->addStretch(0);
        m_layout->setItemSpacing(0, kBaseMargin);

        m_layout->addItem(m_buttonsSeparator);
        m_layout->setItemSpacing(1, kSeparatorMargin);

        m_layout->addItem(actionsLayout);
        m_layout->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
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

    void BookmarkToolTipFrame::setPosition(const QnBookmarksViewer::PosAndBoundsPair &params)
    {
        enum 
        {
            kTailHeight = 10
            , kTailWidth = 20
            , kHalfTailWidth = kTailWidth / 2
            , kTailOffset = 15
            , kSpacerHeight = 2
            , kTailDefaultOffsetLeft = kTailOffset + kHalfTailWidth
            , kTailDefaultOffsetRight = kBookmarkFrameWidth - kTailDefaultOffsetLeft
        };

        setTailWidth(kTailWidth);

        const auto pos = params.first;
        const auto bounds = params.second;

        const bool isFirstItem = !prev();
        const auto height = geometry().height();
        const auto totalHeight = height + (isFirstItem ? kTailHeight : kSpacerHeight);
        const auto currentPos = pos - QPointF(0, totalHeight);

        const auto leftSideDistance = (pos.x() - bounds.first);
        const auto rightSideDistance = (bounds.second - pos.x());

        const qreal finalTailOffset = ((leftSideDistance - kTailDefaultOffsetLeft) < 0 ? leftSideDistance
            : (rightSideDistance - kTailDefaultOffsetRight < 0 ? kBookmarkFrameWidth - rightSideDistance 
                : kTailDefaultOffsetLeft));
        
        if (isFirstItem)
        {
            setTailPos(QPointF(finalTailOffset, totalHeight));
            pointTo(pos);
        }
        else
            setPos(currentPos - QPointF(finalTailOffset, 0));

        if (m_next)
            m_next->setPosition(QnBookmarksViewer::PosAndBoundsPair(currentPos, bounds));
    }

    void BookmarkToolTipFrame::onBookmarkAction(const QString &anchorName)
    {
        const auto callActon = (anchorName == kEditActionAnchorName ? m_editAction : m_removeAction);
        callActon(m_bookmark);
    }

    void BookmarkToolTipFrame::updateBookmark(const QnCameraBookmark &bookmark
        , const QnBookmarkColors &colors)
    {
        setWindowColor(colors.tooltipBackground);
        setFrameColor(colors.tooltipBackground);

        m_buttonsSeparator->setLineColor(colors.separator);
        m_bookmark = bookmark;

        enum { kFirstPosition = 0 };
        enum 
        {
            kMaxHeaderLength = 64
            , kMaxBodyLength = 512
        };

        int position = renewLabel(kFirstPosition, m_name, elideString(bookmark.name, kMaxHeaderLength)
            , this, m_layout, kNameLabelIndex);
        position = renewLabel(position, m_description, elideString(bookmark.description, kMaxBodyLength)
            , this, m_layout, kDescriptionLabelIndex);

        enum { kMaxTags = 16 };
        QStringList tagsList;
        for (const auto &tag: bookmark.tags)
        {
            static const QString tagTemplate = lit("<table cellspacing = \"-1\" cellpadding=\"5\" style = \"margin-top: 4;float: left;display:inline-block; border-style: solid; border-color: %1;border-width:1;\"><tr><td>%2</td></tr></table>");
            tagsList.push_back(tagTemplate.arg(colors.tags.name(QColor::HexRgb), tag));
            if (tagsList.size() >= kMaxTags)
                break;
        }

        static const QString htmlTemplate = lit("<html><body>%1</body></html>");
        const auto tags = (tagsList.empty() ? QString() : htmlTemplate.arg(tagsList.join(lit(""))));
        position = renewLabel(position, m_tags, tags
            , this, m_layout, kTagsIndex);
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
        , kBookmarksResetEventId
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

    bool isHovered() const;

    void setHoverProcessor(HoverFocusProcessor *processor);
    
    ///

    void setColors(const QnBookmarkColors &colors);

    const QnBookmarkColors &colors() const;

private:
    void updatePosition(const QnBookmarksViewer::PosAndBoundsPair &params);

    void updatePositionImpl(const QnBookmarksViewer::PosAndBoundsPair &params);

    void updateBookmarks(QnCameraBookmarkList bookmarks);

    void updateBookmarksImpl(QnCameraBookmarkList bookmarks);

    bool event(QEvent *event) override;

    void emitBookmarkEvent(const QnCameraBookmark &bookmark
        , int eventId);

    void resetBookmarksImpl();

private:
    const GetBookmarksFunc m_getBookmarks;
    const GetPosOnTimelineFunc m_getPos;

    QnBookmarksViewer * const m_owner;
    HoverFocusProcessor *m_hoverProcessor;
    QnBookmarkColors m_colors;

    qint64 m_targetTimestamp;

    QnCameraBookmarkList m_bookmarks;
    BookmarkToolTipFrame *m_headFrame;

    QnBookmarksViewer::PosAndBoundsPair m_futurePosition;
};

enum { kInvalidTimstamp = -1 };

QnBookmarksViewer::Impl::Impl(const GetBookmarksFunc &getBookmarksFunc
    , const GetPosOnTimelineFunc &getPosFunc
    , QnBookmarksViewer *owner)

    : QObject(owner)
    
    , m_getBookmarks(getBookmarksFunc)
    , m_getPos(getPosFunc)

    , m_owner(owner)
    , m_hoverProcessor(nullptr)
    , m_colors()

    , m_targetTimestamp(kInvalidTimstamp)

    , m_bookmarks()
    , m_headFrame(nullptr)

    , m_futurePosition()
{
}

QnBookmarksViewer::Impl::~Impl()
{
}

void QnBookmarksViewer::Impl::setColors(const QnBookmarkColors &colors)
{
    bool bkgChanged = (m_colors.tooltipBackground != colors.background);

    m_colors = colors;
    if (!bkgChanged)
        return;

    BookmarkToolTipFrame *tooltip = m_headFrame;
    while(tooltip)
    {
        tooltip->updateBookmark(tooltip->bookmark(), colors);
        tooltip = tooltip->next();
    }
}

const QnBookmarkColors &QnBookmarksViewer::Impl::colors() const
{
    return m_colors;
}

bool QnBookmarksViewer::Impl::isHovered() const
{
    return (m_hoverProcessor ? m_hoverProcessor->isHovered() : false);
}

void QnBookmarksViewer::Impl::setHoverProcessor(HoverFocusProcessor *processor)
{
    if (m_hoverProcessor == processor)
        return;

    m_hoverProcessor = processor;
    if (!m_hoverProcessor)
        return;

    QObject::connect(m_hoverProcessor, &HoverFocusProcessor::hoverLeft, this, [this]()
    {
        qApp->postEvent(this, new QEvent(static_cast<QEvent::Type>(kBookmarksResetEventId)));
    });
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
        resetBookmarksImpl();
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

    const auto params = m_getPos(m_targetTimestamp);
    const auto bounds = params.second;
    if (params.first.isNull() || ((bounds.second - bounds.first) < kBookmarkFrameWidth))
    {
        updateBookmarksImpl(QnCameraBookmarkList());
        return;
    }

    if (m_bookmarks.empty())
    {
        m_owner->setVisible(false);
        updateBookmarksImpl(m_getBookmarks(m_targetTimestamp));
        updatePosition(params);
    }
    else
        updatePositionImpl(params);
}

void QnBookmarksViewer::Impl::resetBookmarksImpl()
{
    if (m_targetTimestamp == kInvalidTimstamp)
        return;

    m_targetTimestamp = kInvalidTimstamp;
    updateBookmarksImpl(QnCameraBookmarkList());
}


void QnBookmarksViewer::Impl::updateBookmarks(QnCameraBookmarkList bookmarks)
{
    qApp->postEvent(this, new UpdateBokmarksEvent(bookmarks));
}

void QnBookmarksViewer::Impl::updateBookmarksImpl(QnCameraBookmarkList bookmarks)
{
    m_bookmarks = bookmarks;

    int tooltipsCount = 0;

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
            frame->setParentItem(nullptr);
            delete frame;
        }
        else
        {
            ++tooltipsCount;
            frame->updateBookmark(*itNewBookmark, m_colors);
            bookmarks.erase(itNewBookmark);
            lastFrame = frame;
        }

        frame = next;
    }

    /// Adds new frames
    for(const auto& bookmark : bookmarks)
    {
        enum { kMaxTooltipsCount = 6 };
        if (tooltipsCount >= kMaxTooltipsCount)
            break;

        ++tooltipsCount;
        lastFrame = BookmarkToolTipFrame::create(m_colors
            , [this](const QnCameraBookmark &bookmark) { emitBookmarkEvent(bookmark, kBookmarkEditActionEventId); }
            , [this](const QnCameraBookmark &bookmark) { emitBookmarkEvent(bookmark, kBookmarkRemoveActionEventId); }
            , m_hoverProcessor, m_headFrame, lastFrame, m_owner);
        lastFrame->updateBookmark(bookmark, m_colors);
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
    case kBookmarkUpdatePositionEventId:
    {
        if (!m_bookmarks.empty())
            updatePositionImpl(m_futurePosition);

        m_owner->setVisible(true);
        break;
    }
    case kBookmarksResetEventId:
    {
        resetBookmarksImpl();
        break;
    }
    case kBookmarkEditActionEventId:
    {
        const auto bookmarkActionEvent = static_cast<BookmarkActionEvent *>(event);
        emit m_owner->editBookmarkClicked(bookmarkActionEvent->bookmark());
        resetBookmarksImpl();

        break;
    }
    case kBookmarkRemoveActionEventId:
    {
        const auto bookmarkActionEvent = static_cast<BookmarkActionEvent *>(event);
        emit m_owner->removeBookmarkClicked(bookmarkActionEvent->bookmark());
        resetBookmarksImpl();

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
    qApp->postEvent(this, new BookmarkActionEvent(eventId, bookmark));
}

void QnBookmarksViewer::Impl::updatePosition(const QnBookmarksViewer::PosAndBoundsPair &params)
{
    m_futurePosition = params;
    qApp->postEvent(this, new QEvent(static_cast<QEvent::Type>(kBookmarkUpdatePositionEventId)));
}

void QnBookmarksViewer::Impl::updatePositionImpl(const QnBookmarksViewer::PosAndBoundsPair &params)
{
    if (m_headFrame)
        m_headFrame->setPosition(params);
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
    qApp->postEvent(m_impl, new QEvent(static_cast<QEvent::Type>(kBookmarksResetEventId)));
}

void QnBookmarksViewer::setHoverProcessor(HoverFocusProcessor *processor)
{
    m_impl->setHoverProcessor(processor);
}

bool QnBookmarksViewer::isHovered() const
{
    return m_impl->isHovered();
}

const QnBookmarkColors &QnBookmarksViewer::colors() const
{
    return m_impl->colors();
}

void QnBookmarksViewer::setColors(const QnBookmarkColors &colors)
{
    m_impl->setColors(colors);
}

