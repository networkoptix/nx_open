
#include "bookmarks_viewer.h"

#include <core/resource/camera_bookmark.h>

#include <ui/style/skin.h>
#include <ui/common/palette.h>
#include <ui/processors/hover_processor.h>
#include <ui/graphics/items/generic/separator.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/controls/bookmark_tags_control.h>
#include <ui/actions/action_parameters.h>

#include <utils/common/string.h>

#include <QTextDocument>

namespace
{
    enum { kTopPositionIndex = 0 };

    enum 
    {
        kBorderRadius = 2
        , kBookmarkFrameWidth = 250

        , kBaseMargin = 10
        , kBaseHorizontalMargins = kBaseMargin
        , kBaseTopMargin = 12
        , kBaseBottomMargin = kBaseMargin

        , kItemsHorMargin = 4
        , kItemsTopMargin = 0
        , kItemsBottomMargin = 10

        , kTotalHorMargin = kBaseHorizontalMargins + kItemsHorMargin
    };

    enum
    {
        kBookmarksUpdateEventId = QEvent::User + 1
        , kBookmarkUpdatePositionEventId
        , kBookmarksResetEventId
        , kBookmarkEditActionEventId
        , kBookmarkRemoveActionEventId
        , kBookmarkPlayActionEventId
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
        int topMargin;

        LabelParams(bool initBold
            , int initFontSize
            , int initTopPadding);
    };

    LabelParams::LabelParams(bool initBold
        , int initFontSize
        , int initTopMargin)
        : bold(initBold)
        , fontSize(initFontSize)
        , topMargin(initTopMargin)
    {}

    /// Array of parameters for label. Indexed by LabelParamIds enum
    const LabelParams kLabelParams[] = 
    {
        LabelParams(true, 16, 0)        /// For name label
        , LabelParams(false, 12, 4)     /// For description label
        , LabelParams(true, 12, 15)     /// For tags label
    };

    int placeLabel(QnProxyLabel *label
        , const QColor &textColor
        , QGraphicsLinearLayout *layout
        , int insertionIndex
        , LabelParamIds labelParamsId)
    {
        label->setIndent(0);
        label->setMargin(0);
        layout->insertItem(insertionIndex, label);

        const LabelParams &params = kLabelParams[labelParamsId];
        if (insertionIndex > 0 && params.topMargin)
            layout->setItemSpacing(insertionIndex - 1, params.topMargin);


        QFont font = label->font();
        font.setBold(params.bold);
        font.setPixelSize(params.fontSize);
        label->setFont(font);

        setPaletteColor(label, QPalette::Background, Qt::transparent);
        setPaletteColor(label, QPalette::WindowText, textColor);

        const auto labelSize = kBookmarkFrameWidth - kTotalHorMargin * 2;
        label->setWordWrap(true);
        label->setPreferredWidth(labelSize);
        label->setAlignment(Qt::AlignLeft);
        label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);

        if (label->textFormat() == Qt::RichText)    /// Workaround for wrong sizeHint when rendering 'complex' html
        {
            QTextDocument td;
            td.setHtml(label->text());
            td.setTextWidth(label->minimumWidth());
            td.setDocumentMargin(0);

            label->setMaximumHeight(td.documentLayout()->documentSize().height());
            label->setMinimumHeight(td.documentLayout()->documentSize().height());
        }

        return (insertionIndex + 1);
    }

    int createTagsControl(int insertionIndex
        , const QnCameraBookmarkTags &tags
        , const QColor &commonTextColor
        , QGraphicsItem *parent
        , QGraphicsLinearLayout *layout
        , QnBookmarksViewer *viewer
        , LabelParamIds paramsId)
    {
        if (tags.empty())
            return insertionIndex;

        const auto tagsControl = new QnBookmarkTagsControl(tags, parent);

        QObject::connect(tagsControl, &QnBookmarkTagsControl::tagClicked
            , viewer, [viewer](const QString &tag)
        {
            viewer->tagClicked(tag);
            viewer->resetBookmarks();   /// Hides tooltip
        });
        return placeLabel(tagsControl, commonTextColor, layout, insertionIndex, paramsId);
    }

    int createLabel(int insertionIndex
        , const QString &text
        , const QColor &textColor
        , QGraphicsItem *parent
        , QGraphicsLinearLayout *layout
        , LabelParamIds labelParamsId)
    {
        const QString trimmedText = text.trimmed();

        if (text.isEmpty())
            return insertionIndex;
        
        const auto label = new QnProxyLabel(parent);

        label->setText(trimmedText);

        return placeLabel(label, textColor, layout, insertionIndex, labelParamsId);
    }

    void insertButtonsSeparator(const QColor &color
        , int index
        , QGraphicsItem *item
        , QGraphicsLinearLayout *layout)
    {
        enum 
        {
            kButtonSeparatorWidth = 1
            , kSeparatorSpacingBefore = 15 
        };

        const auto separator = new QnSeparator(kButtonSeparatorWidth, color, item);
        if (index > 0)
            layout->setItemSpacing(index - 1, kSeparatorSpacingBefore);
        layout->insertItem(index, separator);
    }

    void insertBookmarksSeparator(int index
        , const QnBookmarkColors &colors
        , QGraphicsItem *parent
        , QGraphicsLinearLayout *layout)
    {
        SeparatorAreas areas(1, SeparatorAreaProperties(2, colors.bookmarksSeparatorTop));
        areas.append(SeparatorAreaProperties(1, colors.bookmarksSeparatorBottom));

        const auto separator = new QnSeparator(areas, parent);
        layout->insertItem(index, separator);
    }

    ///

    void insertMoreItemsMessage(const QString &moreItemsText
        , const QnBookmarkColors &colors
        , QGraphicsLinearLayout *layout
        , QGraphicsItem *parent)
    {
        insertBookmarksSeparator(kTopPositionIndex, colors, parent, layout);

        enum 
        {
            kMoreItemsItemHeight = 40 
            , kFontPixelSize = 11
        };

        auto label = new QnProxyLabel(moreItemsText, parent);
        label->setMinimumSize(kBookmarkFrameWidth, kMoreItemsItemHeight);

        QFont font = label->font();
        font.setPixelSize(kFontPixelSize);
        font.setBold(true);
        label->setFont(font);
        label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

        setPaletteColor(label, QPalette::Background, Qt::transparent);
        setPaletteColor(label, QPalette::WindowText, colors.moreItemsText);

        layout->insertItem(kTopPositionIndex, label);
    }

    ///

    typedef std::function<void (const QnCameraBookmark &bookmark
        , int eventId)> EmitBookmarkEventFunc;

    class BookmarkToolTipFrame : public QnToolTipWidget
    {
        Q_DECLARE_TR_FUNCTIONS(BookmarkToolTipFrame)

    public:
        BookmarkToolTipFrame(const QnCameraBookmarkList &bookmarks
            , bool showMoreTooltip
            , const QnBookmarkColors &colors
            , const EmitBookmarkEventFunc &emitBookmarkEvent
            , QnBookmarksViewer *parent);

        virtual ~BookmarkToolTipFrame();

        void setPosition(const QnBookmarksViewer::PosAndBoundsPair &params);

    private:
        QGraphicsLinearLayout *createBookmarksLayout(const QnCameraBookmark &bookmark
            , const QnBookmarkColors &colors
            , const EmitBookmarkEventFunc &emitBookmarkEvent
            , QnBookmarksViewer *viewer);

        QGraphicsLinearLayout *createLeftCountLayout(int bookmarksLeft
            , const QnBookmarkColors &colors);

    private:
        QGraphicsLinearLayout *m_mainLayout;
    };

    BookmarkToolTipFrame::BookmarkToolTipFrame(const QnCameraBookmarkList &bookmarks
        , bool showMoreTooltip
        , const QnBookmarkColors &colors
        , const EmitBookmarkEventFunc &emitBookmarkEvent
        , QnBookmarksViewer *parent)

        : QnToolTipWidget(parent)

        , m_mainLayout(new QGraphicsLinearLayout(Qt::Vertical))
    {   
        setMaximumWidth(kBookmarkFrameWidth);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);

        setRoundingRadius(kBorderRadius);
        setWindowColor(colors.tooltipBackground);
        setFrameColor(colors.tooltipBackground);

        setLayout(m_mainLayout);
        m_mainLayout->setContentsMargins(0, 0, 0, 0);        
        m_mainLayout->setSpacing(0);

        bool addSeparator = false;
        for (const auto &bookmark: bookmarks)
        {
            if (addSeparator)
                insertBookmarksSeparator(kTopPositionIndex, colors, this, m_mainLayout);

            m_mainLayout->insertItem(kTopPositionIndex
                , createBookmarksLayout(bookmark, colors, emitBookmarkEvent, parent));
            addSeparator = true;
        }

        if (showMoreTooltip)
        {
            const auto message = lit("%1\n%2").arg(tr("Zoom timeline"), tr("to view more bookmarks"));
            insertMoreItemsMessage(message, colors, m_mainLayout, this);
        }
    }

    BookmarkToolTipFrame::~BookmarkToolTipFrame()
    {
    }

    QGraphicsLinearLayout *createVertLayout(int horMargin
        , int topMargin
        , int bottomMargin)
    {
        QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);

        layout->setSpacing(0);
        layout->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        layout->setContentsMargins(horMargin, topMargin, horMargin, bottomMargin);

        return layout;
    }


    QGraphicsLinearLayout *createButtonsLayout(
        const QnCameraBookmark &bookmark
        , const EmitBookmarkEventFunc &emitBookmarkEvent
        , QGraphicsItem *parent)
    {
        auto buttonsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
        buttonsLayout->setSpacing(0);

        const auto createButton = 
            [parent, emitBookmarkEvent, bookmark](const char *iconName , int eventId)
        {
            enum { kSize = 30 };

            auto button = new QnImageButtonWidget(parent);
            button->setIcon(qnSkin->icon(iconName));
            button->setClickableButtons(Qt::LeftButton);
            button->setMaximumSize(kSize, kSize);
            auto speed = button->animationSpeed();

            enum { kAnimationInstantSpeed = 1000 };
            button->setAnimationSpeed(kAnimationInstantSpeed);    // For instant hover state change

            QObject::connect(button, &QnImageButtonWidget::clicked, button
                , [emitBookmarkEvent, eventId, bookmark]()
            {
                emitBookmarkEvent(bookmark, eventId);
            });

            return button;
        };

        buttonsLayout->addItem(createButton("bookmark/tooltip/play.png"
            , kBookmarkPlayActionEventId));
        buttonsLayout->addItem(createButton("bookmark/tooltip/edit.png"
            , kBookmarkEditActionEventId));

        enum { kSpacerStretch = 1000 };
        buttonsLayout->addStretch(kSpacerStretch);
        buttonsLayout->addItem(createButton("bookmark/tooltip/delete.png"
            , kBookmarkRemoveActionEventId));
        return buttonsLayout;
    }

    QGraphicsLinearLayout *BookmarkToolTipFrame::createBookmarksLayout(const QnCameraBookmark &bookmark
        , const QnBookmarkColors &colors
        , const EmitBookmarkEventFunc &emitBookmarkEvent
        , QnBookmarksViewer *viewer)
    {
        const auto layout = createVertLayout(kBaseHorizontalMargins, kBaseTopMargin, kBaseBottomMargin);
        const auto bookmarkItemsLayout = createVertLayout(kItemsHorMargin
            , kItemsTopMargin, kItemsBottomMargin);

        enum 
        {
            kMaxHeaderLength = 64
            , kMaxBodyLength = 512
        };

        enum { kFirstPosition = 0 };
        int position = createLabel(kFirstPosition, elideString(bookmark.name, kMaxHeaderLength)
            , colors.text, this, bookmarkItemsLayout, kNameLabelIndex);

        position = createLabel(position, elideString(bookmark.description, kMaxBodyLength)
            , colors.text, this, bookmarkItemsLayout, kDescriptionLabelIndex);

        if (!bookmark.tags.empty())
        {
            enum { kMaxTags = 16 };
            const auto &trimmedTags = (bookmark.tags.size() <= kMaxTags ? bookmark.tags
                : QnCameraBookmarkTags::fromList(bookmark.tags.toList().mid(0, kMaxTags)));
 
            position = createTagsControl(position, trimmedTags, colors.text, this
                , bookmarkItemsLayout, viewer, kTagsIndex);
        }
       
        if (position)
            insertButtonsSeparator(colors.buttonsSeparator, position, this, bookmarkItemsLayout);

        layout->addItem(bookmarkItemsLayout);
        layout->addItem(createButtonsLayout(bookmark, emitBookmarkEvent, this));
        return layout;
    }

    QGraphicsLinearLayout *BookmarkToolTipFrame::createLeftCountLayout(int bookmarksLeft
        , const QnBookmarkColors &colors)
    {
        return nullptr;
    }

    void BookmarkToolTipFrame::setPosition(const QnBookmarksViewer::PosAndBoundsPair &params)
    {
        enum 
        {
            kTailHeight = 10
            , kTailWidth = 20
            , kHalfTailWidth = kTailWidth / 2
            , kTailOffset = 15
            , kTailDefaultOffsetLeft = kTailOffset + kHalfTailWidth
            , kTailDefaultOffsetRight = kBookmarkFrameWidth - kTailDefaultOffsetLeft
        };

        setTailWidth(kTailWidth);

        const auto pos = params.first;
        const auto bounds = params.second;

        const auto totalHeight = geometry().height() + kTailHeight;
        const auto currentPos = pos - QPointF(0, totalHeight);

        const auto leftSideDistance = (pos.x() - bounds.first);
        const auto rightSideDistance = (bounds.second - pos.x());

        const qreal finalTailOffset = ((leftSideDistance - kTailDefaultOffsetLeft) < 0 ? leftSideDistance
            : (rightSideDistance - kTailDefaultOffsetRight < 0 ? kBookmarkFrameWidth - rightSideDistance 
                : kTailDefaultOffsetLeft));
        
        setTailPos(QPointF(finalTailOffset, totalHeight));
        pointTo(pos);
    }
  
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

    void emitBookmarkEvent(const QnCameraBookmark &bookmark
        , int eventId);

private:
    void updatePosition(const QnBookmarksViewer::PosAndBoundsPair &params);

    void updatePositionImpl(const QnBookmarksViewer::PosAndBoundsPair &params);

    void updateBookmarks(QnCameraBookmarkList bookmarks);

    void updateBookmarksImpl(QnCameraBookmarkList bookmarks);

    bool event(QEvent *event) override;

    void resetBookmarksImpl();

private:
    typedef QScopedPointer<BookmarkToolTipFrame> BookmarkToolTipFramePtr;

    const GetBookmarksFunc m_getBookmarks;
    const GetPosOnTimelineFunc m_getPos;

    BookmarkToolTipFramePtr m_tooltip;

    QnBookmarksViewer * const m_owner;
    HoverFocusProcessor *m_hoverProcessor;
    QnBookmarkColors m_colors;

    qint64 m_targetTimestamp;

    QnCameraBookmarkList m_bookmarks;

    QnBookmarksViewer::PosAndBoundsPair m_futurePosition;
};

enum { kInvalidTimstamp = -1 };

QnBookmarksViewer::Impl::Impl(const GetBookmarksFunc &getBookmarksFunc
    , const GetPosOnTimelineFunc &getPosFunc
    , QnBookmarksViewer *owner)

    : QObject(owner)
    
    , m_getBookmarks(getBookmarksFunc)
    , m_getPos(getPosFunc)

    , m_tooltip()

    , m_owner(owner)
    , m_hoverProcessor(nullptr)
    , m_colors()

    , m_targetTimestamp(kInvalidTimstamp)

    , m_bookmarks()

    , m_futurePosition()
{
}

QnBookmarksViewer::Impl::~Impl()
{
}

void QnBookmarksViewer::Impl::setColors(const QnBookmarkColors &colors)
{
    if (m_colors != colors)
        return;

    m_colors = colors;
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

    if (m_tooltip)
        m_hoverProcessor->addTargetItem(m_tooltip.data());

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
    if (m_bookmarks == bookmarks)
        return;

    m_bookmarks = bookmarks;

    enum { kMaxBookmarksCount = 6 };

    const int bookmarksCount = std::min<int>(m_bookmarks.size(), kMaxBookmarksCount);
    const int bookmarksLeft = m_bookmarks.size() - bookmarksCount;
    const auto &trimmedBookmarks = (bookmarksLeft
        ? m_bookmarks.mid(0, kMaxBookmarksCount) : m_bookmarks);


    if (trimmedBookmarks.empty())
        m_tooltip.reset();
    else
    {
        const auto emitBookmarkEventFunc = [this](const QnCameraBookmark &bookmark, int eventId)
            { emitBookmarkEvent(bookmark, eventId); };

        m_tooltip.reset(new BookmarkToolTipFrame(trimmedBookmarks, (bookmarksLeft > 0)
            , m_colors, emitBookmarkEventFunc, m_owner));
    }

    if (m_tooltip && m_hoverProcessor)
        m_hoverProcessor->addTargetItem(m_tooltip.data());
}

bool QnBookmarksViewer::Impl::event(QEvent *event)
{
    const auto eventType = event->type();
    switch(eventType)
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
    case kBookmarkRemoveActionEventId:
    case kBookmarkPlayActionEventId:
    {
        const auto bookmarkActionEvent = static_cast<BookmarkActionEvent *>(event);
        const auto &bookmark = bookmarkActionEvent->bookmark();
        
        if (eventType == kBookmarkEditActionEventId)
            emit m_owner->editBookmarkClicked(bookmark);
        else if (eventType == kBookmarkRemoveActionEventId)
            emit m_owner->removeBookmarkClicked(bookmark);
        else
            emit m_owner->playBookmark(bookmark);

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
    if (m_tooltip)
        m_tooltip->setPosition(params);
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

