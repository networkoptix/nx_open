// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_tooltip.h"

#include <QtGui/QPainter>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <core/resource/camera_bookmark.h>
#include <flowlayout/flowlayout.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>

namespace nx::vms::client::desktop::workbench::timeline {

namespace {

static constexpr int kWidgetWidth = 252;
static constexpr qsizetype kMaxBookmarksNumber = 3;
static constexpr int kRoundingRadius = 2;
static constexpr BaseTooltip::TailGeometry kTailGeometry = {3, 6, 2};

bool paintButtonFunction(QPainter* painter, const QStyleOption* /*option*/, const QWidget* widget)
{
    const QPushButton* thisButton = qobject_cast<const QPushButton*>(widget);

    QIcon::Mode mode = QnIcon::Normal;

    if (!thisButton->isEnabled())
        mode = QnIcon::Disabled;
    else if (thisButton->isDown())
        mode = QnIcon::Pressed;
    else if (thisButton->underMouse())
        mode = QnIcon::Active;

    thisButton->icon().paint(painter, thisButton->rect(), Qt::AlignCenter,
        mode, thisButton->isChecked() ? QIcon::On : QIcon::Off);

    return true;
}

} // namespace

BookmarkTooltip::BookmarkTooltip(
    const QnCameraBookmarkList& bookmarks,
    bool readOnly,
    QWidget* parent)
    :
    base_type(Qt::BottomEdge, /*filterEvents*/ false, parent),
    m_readOnly(readOnly)
{
    setRoundingRadius(kRoundingRadius);
    setTailGeometry(kTailGeometry);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFixedWidth(kWidgetWidth);

    QVBoxLayout* layout = new QVBoxLayout();

    constexpr auto kMainLayoutLeftRightMargin = 1;
    layout->setContentsMargins(
        kMainLayoutLeftRightMargin, 0, kMainLayoutLeftRightMargin, 2 + tailGeometry().height);
    layout->setSpacing(0);

    bool addSeparator = false;

    if (bookmarks.size() > kMaxBookmarksNumber)
    {
        layout->addWidget(createMoreItemsLabel());
        addSeparator = true;
    }

    for (int i = 0; i < std::min<int>(bookmarks.size(), kMaxBookmarksNumber); ++i)
    {
        const auto& bookmark = bookmarks[i];

        if (addSeparator)
        {
            layout->addWidget(createBookmarkSeparator());
            addSeparator = true;
        }

        constexpr auto kBookmarkLayoutLeftRightMargin = 14;
        QVBoxLayout* topBookmarkLayout = new QVBoxLayout();
        topBookmarkLayout->setContentsMargins(
            kBookmarkLayoutLeftRightMargin, 12, kBookmarkLayoutLeftRightMargin, 0);
        topBookmarkLayout->setSpacing(16);

        QVBoxLayout* nameLayout = new QVBoxLayout();
        nameLayout->setSpacing(4);

        nameLayout->addWidget(createNameLabel(bookmark.name));

        if (!bookmark.description.isEmpty())
        {
            const auto descriptionLabel = createDescriptionLabel(bookmark.description);
            descriptionLabel->setFixedWidth(
                kWidgetWidth - kMainLayoutLeftRightMargin * 2 - kBookmarkLayoutLeftRightMargin * 2);
            nameLayout->addWidget(descriptionLabel);
        }

        topBookmarkLayout->addLayout(nameLayout);

        if (!bookmark.tags.isEmpty())
        {
            auto tagsLayout = new FlowLayout();
            tagsLayout->setSpacing(2, 2);

            enum { kMaxTags = 16 };

            int tagNumber = 0;
            for (const QString& tag: bookmark.tags)
            {
                if (tag.isEmpty())
                    continue;

                if (++tagNumber > kMaxTags)
                    break;

                tagsLayout->addWidget(createTagButton(tag));
            }

            topBookmarkLayout->addLayout(tagsLayout);
        }

        topBookmarkLayout->addWidget(createInternalSeparator());

        layout->addLayout(topBookmarkLayout);

        layout->addLayout(createButtonsLayout(bookmark));
    }

    setLayout(layout);
}

void BookmarkTooltip::setAllowExport(bool allowExport)
{
    m_exportButton->setVisible(allowExport);
}

void BookmarkTooltip::colorizeBorderShape(const QPainterPath& borderShape)
{
    QPainter(this).fillPath(borderShape, QBrush(core::ColorTheme::transparent(
        core::colorTheme()->color("timeline.bookmark.background"), 0.8)));
}

QWidget* BookmarkTooltip::createMoreItemsLabel()
{
    static const auto kMoreItemsCaption = tr("Zoom timeline\nto view more bookmarks",
        "It is highly recommended to split message in two lines");

    auto moreItemsLabel = new QLabel(kMoreItemsCaption, this);
    moreItemsLabel->setMargin(0);
    moreItemsLabel->setAlignment(Qt::AlignCenter);
    moreItemsLabel->setWordWrap(true);
    moreItemsLabel->setStyleSheet(QStringLiteral("color: %1;")
        .arg(core::colorTheme()->color("timeline.bookmark.more_items_label").name()));

    QFont font = moreItemsLabel->font();
    font.setPixelSize(fontConfig()->small().pixelSize());
    font.setBold(true);
    moreItemsLabel->setFont(font);
    moreItemsLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    return moreItemsLabel;
}

QWidget* BookmarkTooltip::createBookmarkSeparator()
{
    QFrame* bookmarkSeparator = new QFrame(this);
    bookmarkSeparator->setFrameShape(QFrame::StyledPanel);
    bookmarkSeparator->setFixedHeight(3);
    bookmarkSeparator->setStyleSheet(QStringLiteral(
        "border-top: 2px solid %1;"
        "border-bottom: 1px solid %2;")
        .arg(core::colorTheme()->color("timeline.bookmark.thick_line").name(),
             core::colorTheme()->color("timeline.bookmark.thin_line").name()));

    return bookmarkSeparator;
}

QWidget* BookmarkTooltip::createNameLabel(const QString& text)
{
    auto nameLabel = new QLabel(text, this);
    nameLabel->setMargin(0);
    nameLabel->setStyleSheet(QStringLiteral("color: %1;")
        .arg(core::colorTheme()->color("timeline.bookmark.label").name()));

    QFont font = nameLabel->font();
    font.setPixelSize(16);
    font.setBold(true);
    nameLabel->setFont(font);

    return nameLabel;
}

QWidget* BookmarkTooltip::createDescriptionLabel(const QString& text)
{
    auto descriptionLabel = new QLabel(text, this);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setStyleSheet(QStringLiteral("color: %1;")
        .arg(core::colorTheme()->color("timeline.bookmark.label").name()));

    QFont font = descriptionLabel->font();
    font.setPixelSize(12);
    descriptionLabel->setFont(font);
    descriptionLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    return descriptionLabel;
}

QWidget* BookmarkTooltip::createTagButton(const QString& tag)
{
    QPushButton* tagButton = new QPushButton(tag, this);
    tagButton->setObjectName("BookmarkTooltipTagButton");
    tagButton->setStyleSheet(QStringLiteral(
        "QPushButton{ padding: 4px; border-radius: 0px; font: bold; font-size: 11px; }"
        "QPushButton:!hover{ background-color: %1; color: %2; }"
        "QPushButton:hover{ background-color: %3; color: %4; }"
        "QPushButton:pressed{ background-color: %5; color: %6; }")
        .arg(core::colorTheme()->color("timeline.bookmark.button.background").name(),
             core::colorTheme()->color("timeline.bookmark.button.text").name(),
             core::colorTheme()->color("timeline.bookmark.button.background_hover").name(),
             core::colorTheme()->color("timeline.bookmark.button.text_hover").name(),
             core::colorTheme()->color("timeline.bookmark.button.background_pressed").name(),
             core::colorTheme()->color("timeline.bookmark.button.text_pressed").name()));
    connect(tagButton, &QPushButton::pressed, [this, tag]
        {
            emit tagClicked(tag);
        });

    return tagButton;
}

QWidget* BookmarkTooltip::createInternalSeparator()
{
    QFrame* internalSeparator = new QFrame(this);
    internalSeparator->setFrameShape(QFrame::StyledPanel);
    internalSeparator->setFixedHeight(1);
    internalSeparator->setStyleSheet(QStringLiteral("border-top: 1px solid %1;")
        .arg(core::colorTheme()->color("timeline.bookmark.thin_line").name()));

    return internalSeparator;
}

QPushButton* BookmarkTooltip::createButton(const char* iconName, const QString& toolTip)
{
    enum { kSize = 30 };

    auto button = new CustomPainted<QPushButton>(this);
    button->setCustomPaintFunction(paintButtonFunction);
    button->setIcon(qnSkin->icon(iconName));
    button->setFixedSize(kSize, kSize);
    button->setToolTip(toolTip);

    return button;
}

QLayout* BookmarkTooltip::createButtonsLayout(const QnCameraBookmark& bookmark)
{
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(0);
    buttonLayout->setContentsMargins(10, 10, 10, 10);

    auto playButton = createButton("bookmark/tooltip/play.png", tr("Play bookmark from the beginning"));
    playButton->setObjectName("BookmarkTooltipPlayButton");
    connect(playButton, &QPushButton::clicked, this,
        [this, bookmark]
        {
            emit playClicked(bookmark);
        });
    buttonLayout->addWidget(playButton);

    if (!m_readOnly)
    {
        auto editButton = createButton("bookmark/tooltip/edit.png", tr("Edit bookmark"));
        editButton->setObjectName("BookmarkTooltipEditButton");
        connect(editButton, &QPushButton::clicked, this,
            [this, bookmark]
            {
                emit editClicked(bookmark);
            });
        buttonLayout->addWidget(editButton);
    }

    m_exportButton = createButton("bookmark/tooltip/export.png", tr("Export bookmark"));
    m_exportButton->setObjectName("BookmarkTooltipExportButton");
    connect(m_exportButton, &QPushButton::clicked, this,
        [this, bookmark]
        {
            emit exportClicked(bookmark);
        });
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addStretch();

    if (!m_readOnly)
    {
        auto deleteButton = createButton("bookmark/tooltip/delete.png", tr("Delete bookmark"));
        deleteButton->setObjectName("BookmarkTooltipDeleteButton");
        connect(deleteButton, &QPushButton::clicked, this,
            [this, bookmark]
            {
                emit deleteClicked(bookmark);
            });
        buttonLayout->addWidget(deleteButton);
    }

    return buttonLayout;
}

} // namespace nx::vms::client::desktop::workbench::timeline
