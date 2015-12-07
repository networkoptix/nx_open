
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
        kTagClickedActionEventId = QEvent::User + 1
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
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);

        setPaletteColor(label, QPalette::Background, Qt::transparent);
        setPaletteColor(label, QPalette::WindowText, colors.moreItemsText);

        layout->insertItem(kTopPositionIndex, label);
    }

    ///

    typedef std::function<void (const QString &tag
        , int eventId)> EmitTagEventFunc;
    typedef std::function<void (const QnCameraBookmark &bookmark
        , int eventId)> EmitBookmarkEventFunc;

    /// TODO: #ynikitenkov Move to separate file (for signals etc)
    class BookmarkToolTipFrame : public QnToolTipWidget
    {
        Q_DECLARE_TR_FUNCTIONS(BookmarkToolTipFrame)

    public:
        BookmarkToolTipFrame(const QnCameraBookmarkList &bookmarks
            , bool showMoreTooltip
            , const QnBookmarkColors &colors
            , const EmitTagEventFunc &emitTagEvent
            , const EmitBookmarkEventFunc &emitBookmarkEvent
            , QnBookmarksViewer *viewer);

        virtual ~BookmarkToolTipFrame();

        void setPosition(const QnBookmarksViewer::PosAndBoundsPair &params);

    private:
        void updatePosition();

    private:
        int createTagsControl(int insertionIndex
            , const QnCameraBookmarkTags &tags
            , const QColor &commonTextColor
            , QGraphicsLinearLayout *layout);

        QGraphicsLinearLayout *createButtonsLayout(const QnCameraBookmark &bookmark);

        QGraphicsLinearLayout *createBookmarksLayout(const QnCameraBookmark &bookmark
            , const QnBookmarkColors &colors);

        QGraphicsLinearLayout *createLeftCountLayout(int bookmarksLeft
            , const QnBookmarkColors &colors);

    private:
        const EmitTagEventFunc m_emitTagEvent;
        const EmitBookmarkEventFunc m_emitBookmarkEvent;
        QnBookmarksViewer * const m_viewer;
        QGraphicsLinearLayout *m_mainLayout;
        QnBookmarksViewer::PosAndBoundsPair m_posOnTimeline;
    };

    BookmarkToolTipFrame::BookmarkToolTipFrame(const QnCameraBookmarkList &bookmarks
        , bool showMoreTooltip
        , const QnBookmarkColors &colors
        , const EmitTagEventFunc &emitTagEvent
        , const EmitBookmarkEventFunc &emitBookmarkEvent
        , QnBookmarksViewer *viewer)

        : QnToolTipWidget(viewer)

        , m_emitTagEvent(emitTagEvent)
        , m_emitBookmarkEvent(emitBookmarkEvent)
        , m_viewer(viewer)
        , m_mainLayout(new QGraphicsLinearLayout(Qt::Vertical))
        , m_posOnTimeline()
    {
        setMaximumWidth(kBookmarkFrameWidth);
        setMinimumWidth(kBookmarkFrameWidth);
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
                , createBookmarksLayout(bookmark, colors));
            addSeparator = true;
        }

        if (showMoreTooltip)
        {
            static const auto kMoreItemsCaption = tr("Zoom timeline\nto view more bookmarks", "Use '\n' to split message in two lines (required)");
            insertMoreItemsMessage(kMoreItemsCaption, colors, m_mainLayout, this);
        }

        connect(this, &BookmarkToolTipFrame::geometryChanged, this, [this]()
        {
            updatePosition();
        });
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

    int BookmarkToolTipFrame::createTagsControl(int insertionIndex
        , const QnCameraBookmarkTags &tags
        , const QColor &commonTextColor
        , QGraphicsLinearLayout *layout)
    {
        if (tags.empty())
            return insertionIndex;

        const auto tagsControl = new QnBookmarkTagsControl(
            tags, m_viewer->allowClickOnTag(), this);

        QObject::connect(tagsControl, &QnBookmarkTagsControl::tagClicked
            , this, [this](const QString &tag)
        {
            m_emitTagEvent(tag, kTagClickedActionEventId);
        });

        return placeLabel(tagsControl, commonTextColor, layout, insertionIndex, kTagsIndex);
    }

    QGraphicsLinearLayout *BookmarkToolTipFrame::createButtonsLayout(const QnCameraBookmark &bookmark)
    {
        auto buttonsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
        buttonsLayout->setMinimumWidth(kBookmarkFrameWidth - kBaseMargin * 2);
        buttonsLayout->setSpacing(0);

        const auto createButton =
            [this, bookmark](const char *iconName , int eventId)
        {
            enum { kSize = 30 };

            auto button = new QnImageButtonWidget(this);
            button->setIcon(qnSkin->icon(iconName));
            button->setClickableButtons(Qt::LeftButton);
            button->setMaximumSize(kSize, kSize);

            enum { kAnimationInstantSpeed = 1000 };
            button->setAnimationSpeed(kAnimationInstantSpeed);    // For instant hover state change

            QObject::connect(button, &QnImageButtonWidget::clicked, button
                , [this, eventId, bookmark]() { m_emitBookmarkEvent(bookmark, eventId); });

            return button;
        };

        buttonsLayout->addItem(createButton("bookmark/tooltip/play.png"
            , kBookmarkPlayActionEventId));

        if (!m_viewer->readOnly())
        {
            buttonsLayout->addItem(createButton("bookmark/tooltip/edit.png"
                , kBookmarkEditActionEventId));

            enum { kSpacerStretch = 1000 };
            buttonsLayout->addStretch(kSpacerStretch);
            buttonsLayout->addItem(createButton("bookmark/tooltip/delete.png"
                , kBookmarkRemoveActionEventId));
        }
        return buttonsLayout;
    }

    QGraphicsLinearLayout *BookmarkToolTipFrame::createBookmarksLayout(const QnCameraBookmark &bookmark
        , const QnBookmarkColors &colors)
    {
        const auto layout = createVertLayout(kBaseHorizontalMargins, kBaseTopMargin, kBaseBottomMargin);
        const auto bookmarkItemsLayout = createVertLayout(kItemsHorMargin
            , kItemsTopMargin, kItemsBottomMargin);

        enum
        {
            kMaxHeaderLength = 64
            , kMaxBodyLength = 96
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

            position = createTagsControl(position, trimmedTags, colors.text, bookmarkItemsLayout);
        }

        if (position)
            insertButtonsSeparator(colors.buttonsSeparator, position, this, bookmarkItemsLayout);

        layout->addItem(bookmarkItemsLayout);
        layout->addItem(createButtonsLayout(bookmark));
        return layout;
    }

    void BookmarkToolTipFrame::setPosition(const QnBookmarksViewer::PosAndBoundsPair &params)
    {
        m_posOnTimeline = params;
        updatePosition();
    }

    void BookmarkToolTipFrame::updatePosition()
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

        const auto pos = m_posOnTimeline.first;
        const auto bounds = m_posOnTimeline.second;

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

    class TagActionEvent : public QEvent
    {
    public:
        TagActionEvent(int eventId
            , const QString &tag);

        virtual ~TagActionEvent();

        const QString &tag() const;

    private:
        const QString m_tag;
    };

    TagActionEvent::TagActionEvent(int eventId
        , const QString &tag)
        : QEvent(static_cast<QEvent::Type>(eventId))
        , m_tag(tag)
    {}

    TagActionEvent::~TagActionEvent() {}

    const QString &TagActionEvent::tag() const
    {
        return m_tag;
    }

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

    bool readOnly() const;

    void setReadOnly(bool readonly);

    bool allowClickOnTag() const;

    void setAllowClickOnTag(bool allow);

    void setTargetTimestamp(qint64 timestamp);

    void updateOnWindowChange();

    ///

    bool isHovered() const;

    void setHoverProcessor(HoverFocusProcessor *processor);

    ///

    void setColors(const QnBookmarkColors &colors);

    const QnBookmarkColors &colors() const;

    void resetBookmarks();

private:
    void updatePosition(const QnBookmarksViewer::PosAndBoundsPair &params);

    void updateBookmarks(QnCameraBookmarkList bookmarks);

    bool event(QEvent *event) override;

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
    bool m_readonly;
    bool m_allowClickOnTag;
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
    , m_readonly(false)
    , m_allowClickOnTag(false)
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
        resetBookmarks();
    });
}

bool QnBookmarksViewer::Impl::readOnly() const
{
    return m_readonly;
}

void QnBookmarksViewer::Impl::setReadOnly(bool readonly)
{
    m_readonly = readonly;
}

bool QnBookmarksViewer::Impl::allowClickOnTag() const
{
    return m_allowClickOnTag;
}

void QnBookmarksViewer::Impl::setAllowClickOnTag(bool allow)
{
    m_allowClickOnTag = allow;
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

    m_targetTimestamp = timestamp;
    updateOnWindowChange();
}

void QnBookmarksViewer::Impl::updateOnWindowChange()
{
    if (m_targetTimestamp == kInvalidTimstamp)
        return;

    const auto params = m_getPos(m_targetTimestamp);
    const auto bounds = params.second;
    if (params.first.isNull() || ((bounds.second - bounds.first) < kBookmarkFrameWidth))
    {
        updateBookmarks(QnCameraBookmarkList());
        return;
    }

    updateBookmarks(m_getBookmarks(m_targetTimestamp));

    updatePosition(params);
}

void QnBookmarksViewer::Impl::resetBookmarks()
{
    if (m_targetTimestamp == kInvalidTimstamp)
        return;

    m_targetTimestamp = kInvalidTimstamp;
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
    const auto &trimmedBookmarks = (bookmarksLeft
        ? m_bookmarks.mid(0, kMaxBookmarksCount) : m_bookmarks);


    if (trimmedBookmarks.empty())
        m_tooltip.reset();
    else
    {
        // TODO: #ynikitenkov Replace emitBookmarkEventFunc,
        // emitTagEventFunc with Qt::QueuedConnection later
        const auto emitBookmarkEventFunc= [this](const QnCameraBookmark &bookmark, int eventId)
        {
            qApp->postEvent(this, new BookmarkActionEvent(eventId, bookmark));
        };

        const auto emitTagEventFunc = [this](const QString &tag, int eventId)
        {
            qApp->postEvent(this, new TagActionEvent(eventId, tag));
        };

        m_tooltip.reset(new BookmarkToolTipFrame(trimmedBookmarks, (bookmarksLeft > 0)
            , m_colors, emitTagEventFunc, emitBookmarkEventFunc, m_owner));
    }

    if (m_tooltip && m_hoverProcessor)
        m_hoverProcessor->addTargetItem(m_tooltip.data());
}

bool QnBookmarksViewer::Impl::event(QEvent *event)
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
    {
        const auto bookmarkActionEvent = static_cast<BookmarkActionEvent *>(event);
        const auto &bookmark = bookmarkActionEvent->bookmark();

        if (eventType == kBookmarkEditActionEventId)
            emit m_owner->editBookmarkClicked(bookmark);
        else if (eventType == kBookmarkRemoveActionEventId)
            emit m_owner->removeBookmarkClicked(bookmark);
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

void QnBookmarksViewer::Impl::updatePosition(const QnBookmarksViewer::PosAndBoundsPair &params)
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

bool QnBookmarksViewer::readOnly() const
{
    return m_impl->readOnly();
}

void QnBookmarksViewer::setReadOnly(bool readonly)
{
    m_impl->setReadOnly(readonly);
}

bool QnBookmarksViewer::allowClickOnTag() const
{
    return m_impl->allowClickOnTag();
}

void QnBookmarksViewer::setAllowClickOnTag(bool allow)
{
    m_impl->setAllowClickOnTag(allow);
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

