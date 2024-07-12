// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_tile.h"
#include "ui_event_tile.h"

#include <chrono>
#include <functional>

#include <QtCore/QScopedPointer>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtGui/QPainter>

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/image_providers/resource_thumbnail_provider.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/close_button.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/common/html/html.h>
#include <ui/common/palette.h>

#include "private/event_tile_p.h"

using namespace nx::vms::common::html;
using namespace std::chrono;

namespace nx::vms::client::desktop {

namespace {

// Delay after which preview is requested again in case of receiving "NO DATA".
static const milliseconds kPreviewReloadDelay = seconds(ini().rightPanelPreviewReloadDelay);

static constexpr auto kRoundingRadius = 2;

static constexpr int kMaxNumberOfDisplayedAttributes = 4;

static constexpr auto kTitleFontWeight = QFont::Medium;
static constexpr auto kTitleFontSize = 14;
static constexpr auto kTimestampFontWeight = QFont::Normal;
static constexpr auto kDescriptionFontWeight = QFont::Normal;
static constexpr auto kResourceListFontWeight = QFont::Medium;
static constexpr auto kFooterFontWeight = QFont::Normal;

static constexpr int kProgressBarResolution = 1000;

static constexpr int kSeparatorHeight = 6;

static constexpr int kMaximumPreviewHeightWithHeader = 135;

static constexpr QMargins kMarginsWithHeader(10, 10, 10, 10);

static constexpr QMargins kWidePreviewMarginsWithHeader(0, 0, 0, 0);

static constexpr QMargins kNarrowPreviewMarginsWithHeader(0, 2, 0, 0);

// Close button margins are fine-tuned in correspondence with tile layout.
static constexpr QMargins kCloseButtonMarginsWithHeader(0, 10, 10, 0);

static constexpr auto kDefaultReloadMode = AsyncImageWidget::ReloadMode::showPreviousImage;

constexpr auto kDotRadius = 8;
constexpr auto kDotSpacing = 4;

} // namespace

EventTile::EventTile(QWidget* parent):
    base_type(parent, Qt::FramelessWindowHint),
    d(new Private(this)),
    ui(new Ui::EventTile())
{
    ui->setupUi(this);
    setAttribute(Qt::WA_Hover);

    setPaletteColor(this, QPalette::Base, core::colorTheme()->color("dark5"));
    setPaletteColor(this, QPalette::Light, core::colorTheme()->color("light7"));
    setPaletteColor(this, QPalette::WindowText, core::colorTheme()->color("light16"));
    setPaletteColor(this, QPalette::Text, core::colorTheme()->color("light4"));
    setPaletteColor(this, QPalette::Highlight, core::colorTheme()->color("brand_core"));

    d->closeButton->setHidden(true);
    d->closeButtonAnchor->setMargins(kCloseButtonMarginsWithHeader);

    ui->mainWidget->layout()->setContentsMargins(kMarginsWithHeader);
    ui->wideHolder->layout()->setContentsMargins(kWidePreviewMarginsWithHeader);
    ui->narrowHolder->layout()->setContentsMargins(kNarrowPreviewMarginsWithHeader);
    ui->imagePreviewWidget->setMaximumHeight(kMaximumPreviewHeightWithHeader);
    ui->videoPreviewWidget->setMinimumHeight(kMaximumPreviewHeightWithHeader);
    ui->videoPreviewWidget->setMaximumHeight(kMaximumPreviewHeightWithHeader);

    ui->descriptionLabel->hide();
    ui->debugPreviewTimeLabel->hide();
    ui->timestampLabel->hide();
    ui->actionHolder->hide();
    ui->attributeTable->hide();
    ui->footerLabel->hide();
    ui->progressDescriptionLabel->hide();
    ui->narrowHolder->hide();
    ui->wideHolder->hide();

    auto dots = ui->imagePreviewWidget->busyIndicator()->dots();
    dots->setDotRadius(kDotRadius);
    dots->setDotSpacing(kDotSpacing);

    ui->imagePreviewWidget->setAutoScaleUp(true);
    ui->imagePreviewWidget->setReloadMode(kDefaultReloadMode);
    ui->imagePreviewWidget->setCropMode(AsyncImageWidget::CropMode::always);

    setPaletteColor(ui->imagePreviewWidget, QPalette::Window, core::colorTheme()->color("dark7"));
    setPaletteColor(ui->imagePreviewWidget, QPalette::WindowText, core::colorTheme()->color("dark16"));

    ui->nameLabel->setForegroundRole(QPalette::Text);
    ui->timestampLabel->setForegroundRole(QPalette::WindowText);
    ui->descriptionLabel->setForegroundRole(QPalette::Light);
    ui->debugPreviewTimeLabel->setForegroundRole(QPalette::Light);
    ui->resourceListLabel->setForegroundRole(QPalette::Light);
    ui->footerLabel->setForegroundRole(QPalette::Light);

    QFont font;
    font.setWeight(kTitleFontWeight);
    font.setPixelSize(kTitleFontSize);
    ui->nameLabel->setFont(font);
    ui->nameLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->nameLabel->setOpenExternalLinks(false);

    font.setWeight(kTimestampFontWeight);
    font.setPixelSize(fontConfig()->small().pixelSize());
    ui->timestampLabel->setFont(font);
    ui->timestampLabel->setProperty(style::Properties::kDontPolishFontProperty, true);

    font.setWeight(kDescriptionFontWeight);
    font.setPixelSize(fontConfig()->small().pixelSize());
    ui->descriptionLabel->setFont(font);
    ui->descriptionLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->descriptionLabel->setOpenExternalLinks(false);
    ui->progressDescriptionLabel->setFont(font);
    ui->progressDescriptionLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->progressDescriptionLabel->setOpenExternalLinks(false);
    ui->debugPreviewTimeLabel->setFont(font);
    ui->debugPreviewTimeLabel->setProperty(style::Properties::kDontPolishFontProperty, true);

    font.setWeight(kResourceListFontWeight);
    ui->resourceListLabel->setFont(font);
    ui->resourceListLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->resourceListLabel->setOpenExternalLinks(false);

    font.setWeight(kFooterFontWeight);
    font.setPixelSize(fontConfig()->small().pixelSize());
    ui->attributeTable->setFont(font);
    ui->attributeTable->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->attributeTable->setMaximumNumberOfRows(kMaxNumberOfDisplayedAttributes);
    ui->footerLabel->setFont(font);
    ui->footerLabel->setProperty(style::Properties::kDontPolishFontProperty, true);

    ui->busyIndicator->setContentsMargins(
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding);

    ui->busyIndicator->hide();
    ui->progressHolder->hide();
    ui->mainWidget->hide();
    d->updatePreviewsVisibility();

    QFont progressLabelFont;
    progressLabelFont.setWeight(QFont::Medium);

    d->progressLabel->setParent(ui->progressBar);
    d->progressLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    d->progressLabel->setFont(progressLabelFont);
    d->progressLabel->setForegroundRole(QPalette::Highlight);

    ui->nameLabel->setText({});
    ui->descriptionLabel->setText({});
    ui->resourceListLabel->setText({});
    ui->attributeTable->setContent({});
    ui->footerLabel->setText({});
    ui->timestampLabel->setText({});

    ui->secondaryTimestampHolder->show();
    ui->imagePreviewWidget->setMaximumHeight(kMaximumPreviewHeightWithHeader);
    ui->videoPreviewWidget->setMinimumHeight(kMaximumPreviewHeightWithHeader);
    ui->videoPreviewWidget->setMaximumHeight(kMaximumPreviewHeightWithHeader);
    ui->mainWidget->layout()->setContentsMargins(kMarginsWithHeader);
    ui->wideHolder->layout()->setContentsMargins(kWidePreviewMarginsWithHeader);
    ui->narrowHolder->layout()->setContentsMargins(kNarrowPreviewMarginsWithHeader);
    d->closeButtonAnchor->setMargins(kCloseButtonMarginsWithHeader);

    static constexpr int kProgressLabelShift = 8;
    anchorWidgetToParent(d->progressLabel, {0, 0, 0, kProgressLabelShift});

    ui->progressBar->setRange(0, kProgressBarResolution);
    ui->progressBar->setValue(0);

    ui->nameLabel->ensurePolished();
    d->defaultTitlePalette = ui->nameLabel->palette();

    connect(d->closeButton, &QAbstractButton::clicked, this,
        [this]
        {
            if (d->onCloseAction)
                d->onCloseAction->trigger();

            emit closeRequested();
        });

    const auto activateLink =
        [this](const QString& link)
        {
            if (link.contains(lit("://")))
                QDesktopServices::openUrl(link);
            else
                emit linkActivated(link);
        };

    connect(ui->nameLabel, &QLabel::linkActivated, this, activateLink);
    connect(ui->descriptionLabel, &QLabel::linkActivated, this, activateLink);

    ui->nameLabel->installEventFilter(this);
    ui->resourceListLabel->installEventFilter(this);
    ui->descriptionLabel->installEventFilter(this);
}

EventTile::~EventTile()
{
}

bool EventTile::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::Resize)
    {
        if (object == ui->nameLabel)
            d->updateLabelForCurrentWidth(ui->nameLabel, d->titleLabelDescriptor);
        if (object == ui->descriptionLabel)
            d->updateLabelForCurrentWidth(ui->descriptionLabel, d->descriptionLabelDescriptor);
        if (object == ui->resourceListLabel)
            d->updateLabelForCurrentWidth(ui->resourceListLabel, d->resourceLabelDescriptor);
    }

    return base_type::eventFilter(object, event);
}

bool EventTile::closeable() const
{
    return d->closeable;
}

void EventTile::setCloseable(bool value)
{
    if (d->closeable == value)
        return;

    d->closeable = value;

    if (progressBarVisible())
    {
        QMargins parentMargins = ui->progressBar->parentWidget()->contentsMargins();
        parentMargins.setRight(d->closeable ? d->closeButton->width() : kMarginsWithHeader.right());
        ui->progressBar->parentWidget()->setContentsMargins(parentMargins);
    }

    d->handleStateChanged(underMouse() ? Private::State::hoverOn : Private::State::hoverOff);
}

QString EventTile::title() const
{
    return ui->nameLabel->text();
}

void EventTile::setTitle(const QString& value)
{
    d->setTitle(value);
    ui->mainWidget->setHidden(value.isEmpty());
}

QColor EventTile::titleColor() const
{
    ui->nameLabel->ensurePolished();
    return ui->nameLabel->palette().color(ui->nameLabel->foregroundRole());
}

void EventTile::setTitleColor(const QColor& value)
{
    if (titleColor() == value)
        return;

    if (value.isValid())
        setPaletteColor(ui->nameLabel, ui->nameLabel->foregroundRole(), value);
    else
        ui->nameLabel->setPalette(d->defaultTitlePalette);

    d->updateIcon();
}

QString EventTile::description() const
{
    return ui->descriptionLabel->text();
}

void EventTile::setDescription(const QString& value)
{
    d->setDescription(value);
    ui->descriptionLabel->setHidden(value.isEmpty());
    ui->progressDescriptionLabel->setText(value);
    ui->progressDescriptionLabel->setHidden(value.isEmpty());
}

void EventTile::setResourceList(const QnResourceList& list, const QString& cloudSystemId)
{
    QStringList items;
    auto systemName = d->getFormattedSystemNameIfNeeded(cloudSystemId);
    for (const auto& resource: list)
    {
        NX_ASSERT(resource, "Null resource pointer is an abnormal situation.");
        items.push_back(resource ? toHtmlEscaped(systemName + resource->getName())
                                 : toHtmlEscaped(systemName) + "?");
    }

    if (items.isEmpty() && !cloudSystemId.isEmpty())
        items.push_back(d->getSystemName(cloudSystemId));

    d->setResourceList(items);
}

void EventTile::setResourceList(const QStringList& list, const QString& cloudSystemId)
{
    auto systemName = d->getFormattedSystemNameIfNeeded(cloudSystemId);
    QStringList items = list;
    for (auto& item: items)
        item = toHtml(systemName + item);

    if (items.isEmpty() && !cloudSystemId.isEmpty())
        items.push_back(d->getSystemName(cloudSystemId));

    d->setResourceList(items);
}

QString EventTile::footerText() const
{
    return ui->footerLabel->text();
}

void EventTile::setFooterText(const QString& value)
{
    ui->footerLabel->setText(value);
    ui->footerLabel->setHidden(!d->footerEnabled || value.isEmpty());
}

core::analytics::AttributeList EventTile::attributeList() const
{
    return ui->attributeTable->content();
}

void EventTile::setAttributeList(const core::analytics::AttributeList& value)
{
    ui->attributeTable->setContent(value);
    ui->attributeTable->setHidden(!d->footerEnabled || value.empty());
}

QString EventTile::timestamp() const
{
    return ui->timestampLabel->text();
}

void EventTile::setTimestamp(const QString& value)
{
    ui->timestampLabel->setText(value);
    ui->timestampLabel->setHidden(value.isEmpty());
}

QString EventTile::iconPath() const
{
    return d->iconPath;
}

void EventTile::setIconPath(const QString& value)
{
    if (d->iconPath == value)
        return;

    d->iconPath = value;
    d->updateIcon();
}

core::ImageProvider* EventTile::imageProvider() const
{
    return ui->imagePreviewWidget->imageProvider();
}


void EventTile::setImageProvider(core::ImageProvider* value, bool forceUpdate)
{
    if (imageProvider() == value && !forceUpdate)
        return;

    if (imageProvider())
        imageProvider()->disconnect(this);

    if (value)
        value->loadAsync();

    ui->imagePreviewWidget->setImageProvider(value);
    d->updatePreviewsVisibility();

    d->isPreviewLoadNeeded = false;
    d->forceNextPreviewUpdate = forceUpdate;
    d->updatePreview(/*delay*/ 0ms);

    if (core::ini().showDebugTimeInformationInEventSearchData)
        d->showDebugPreviewTimestamp();

    if (!imageProvider())
        return;

    connect(imageProvider(), &core::ImageProvider::statusChanged, this,
        [this](core::ThumbnailStatus status)
        {
            if (status != core::ThumbnailStatus::Invalid)
                d->isPreviewLoadNeeded = false;

            if (status == core::ThumbnailStatus::NoData && kPreviewReloadDelay > 0s)
                d->updatePreview(kPreviewReloadDelay);

            if (core::ini().showDebugTimeInformationInEventSearchData)
                d->showDebugPreviewTimestamp();
        });
}

void EventTile::setVideoPreviewResource(const QnVirtualCameraResourcePtr& camera)
{
    ui->videoPreviewWidget->setCamera(camera);

    d->updatePreviewsVisibility();
}

void EventTile::setPlaceholder(const QString& text)
{
    ui->imagePreviewWidget->setPlaceholder(text);

    if (!text.isEmpty())
       setImageProvider(nullptr, /*forceUpdate*/ true);
}

void EventTile::setForcePreviewLoader(bool force)
{
    ui->imagePreviewWidget->setReloadMode(force
        ? AsyncImageWidget::ReloadMode::showLoadingIndicator
        : kDefaultReloadMode);

    ui->imagePreviewWidget->setForceLoadingIndicator(force);
}

QRectF EventTile::previewHighlightRect() const
{
    return ui->imagePreviewWidget->highlightRect();
}

void EventTile::setPreviewHighlightRect(const QRectF& relativeRect)
{
    ui->imagePreviewWidget->setHighlightRect(relativeRect);
}

bool EventTile::isPreviewLoadNeeded() const
{
    return d->isPreviewNeeded() && d->isPreviewLoadNeeded;
}

CommandActionPtr EventTile::action() const
{
    return CommandAction::linkedCommandAction(d->action.get());
}

void EventTile::setAction(const CommandActionPtr& value)
{
    d->action.reset(CommandAction::createQtAction(value));
    ui->actionButton->setAction(d->action.get());
    ui->actionHolder->setHidden(!d->action);
}

CommandActionPtr EventTile::additionalAction() const
{
    return CommandAction::linkedCommandAction(d->additionalAction.get());
}

void EventTile::setAdditionalAction(const CommandActionPtr& value)
{
    d->additionalAction.reset(CommandAction::createQtAction(value));
    ui->additionalActionButton->setAction(d->additionalAction.get());
    ui->additionalActionButton->setHidden(!d->additionalAction);
    ui->additionalActionButton->setFlat(true);
}

void EventTile::setOnCloseAction(const CommandActionPtr& value)
{
    d->onCloseAction.reset(CommandAction::createQtAction(value));
}

void EventTile::paintEvent(QPaintEvent* /*event*/)
{
    if (ui->mainWidget->isHidden() && ui->progressHolder->isHidden())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().brush(backgroundRole()));
    painter.drawRoundedRect(rect(), kRoundingRadius, kRoundingRadius);
}

QSize EventTile::minimumSizeHint() const
{
    return base_type::minimumSizeHint().expandedTo({0, kSeparatorHeight});
}

bool EventTile::event(QEvent* event)
{
    if (event->type() == QEvent::Polish)
    {
        const auto result = base_type::event(event);
        d->updatePalette();
        return result;
    }

    const auto closeToStart =
        [this](const QPoint& point)
        {
            return (point - d->clickPoint).manhattanLength() < qApp->startDragDistance();
        };

    switch (event->type())
    {
        case QEvent::Enter:
        case QEvent::HoverEnter:
        {
            d->handleStateChanged(Private::State::hoverOn);
            break;
        }

        case QEvent::Leave:
        case QEvent::HoverLeave:
        case QEvent::Hide:
        {
            d->handleStateChanged(Private::State::hoverOff);
            break;
        }

        case QEvent::Show:
        {
            d->handleStateChanged(
                underMouse() ? Private::State::hoverOn : Private::State::hoverOff);
            break;
        }

        case QEvent::MouseButtonPress:
        {
            const auto mouseEvent = static_cast<QMouseEvent*>(event);
            base_type::event(event);
            event->accept();
            d->clickButton = mouseEvent->button();
            d->clickModifiers = mouseEvent->modifiers();
            d->clickPoint = mouseEvent->pos();
            d->handleStateChanged(Private::State::pressed);
            return true;
        }

        case QEvent::MouseButtonRelease:
        {
            const auto mouseEvent = static_cast<QMouseEvent*>(event);
            if ((mouseEvent->button() == d->clickButton) && closeToStart(mouseEvent->pos()))
                emit clicked(d->clickButton, mouseEvent->modifiers() & d->clickModifiers);
            d->clickButton = Qt::NoButton;
            d->handleStateChanged(
                underMouse() ? Private::State::hoverOn : Private::State::hoverOff);
            break;
        }

        case QEvent::MouseButtonDblClick:
            d->clickButton = Qt::NoButton;
            if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton)
                emit doubleClicked();
            break;

        // Child widgets can capture mouse, so drag is handled in HoverMove instead of MouseMove.
        case QEvent::HoverMove:
        {
            const auto hoverEvent = static_cast<QHoverEvent*>(event);
            if ((d->clickButton == Qt::NoButton) || closeToStart(hoverEvent->pos()))
                break;
            if (d->clickButton == Qt::LeftButton)
                emit dragStarted(d->clickPoint, size());
            d->clickButton = Qt::NoButton;
            break;
        }

        default:
            break;
    }

    return base_type::event(event);
}

bool EventTile::busyIndicatorVisible() const
{
    return !ui->busyIndicator->isHidden();
}

void EventTile::setBusyIndicatorVisible(bool value)
{
    ui->busyIndicator->setVisible(value);
}

bool EventTile::progressBarVisible() const
{
    return !ui->progressHolder->isHidden();
}

void EventTile::setProgressBarVisible(bool value)
{
    ui->progressHolder->setVisible(value);
}

std::optional<qreal> EventTile::progressValue() const
{
    return d->progressValue;
}

void EventTile::setIndefiniteProgress()
{
    if (!d->progressValue)
        return;

    d->progressValue = {};
    ui->progressBar->setRange(0, 0);
    ui->progressBar->setValue(0);
}

void EventTile::setProgressValue(qreal value)
{
    if (d->progressValue && qFuzzyIsNull(*d->progressValue - value))
        return;

    d->progressValue = value;

    ui->progressBar->setRange(0, kProgressBarResolution);
    ui->progressBar->setValue(int(value * kProgressBarResolution));
}

QString EventTile::progressTitle() const
{
    return d->progressLabel->text();
}

void EventTile::setProgressTitle(const QString& value)
{
    d->progressLabel->setText(value);
}

QString EventTile::progressFormat() const
{
    return ui->progressBar->format();
}

void EventTile::setProgressFormat(const QString& value)
{
    if (value.isNull())
    {
        ui->progressBar->resetFormat();
    }
    else
    {
        ui->progressBar->setFormat(value);
    }
}

bool EventTile::previewEnabled() const
{
    return d->previewEnabled;
}

void EventTile::setPreviewEnabled(bool value)
{
    if (d->previewEnabled == value)
        return;

    d->previewEnabled = value;

    d->updatePreviewsVisibility();

    d->updatePreview(/*delay*/ 0ms);
}

bool EventTile::footerEnabled() const
{
    return d->footerEnabled;
}

void EventTile::setFooterEnabled(bool value)
{
    d->footerEnabled = value;
    ui->attributeTable->setHidden(!d->footerEnabled || ui->attributeTable->content().empty());
    ui->footerLabel->setHidden(!d->footerEnabled || ui->footerLabel->text().isEmpty());
}

EventTile::Mode EventTile::mode() const
{
    if (ui->imagePreviewWidget->parentWidget() == ui->narrowHolder)
        return Mode::standard;

    NX_ASSERT(ui->imagePreviewWidget->parentWidget() == ui->wideHolder);
    return Mode::wide;
}

void EventTile::setMode(Mode value)
{
    if (mode() == value)
        return;

    const bool noPreview = d->previewEnabled;
    switch (value)
    {
        case Mode::standard:
            d->setWidgetHolder(ui->imagePreviewWidget, ui->narrowHolder);
            d->setWidgetHolder(ui->videoPreviewWidget, ui->narrowHolder);
            ui->narrowHolder->setHidden(noPreview);
            ui->wideHolder->setHidden(true);
            break;

        case Mode::wide:
            d->setWidgetHolder(ui->imagePreviewWidget, ui->wideHolder);
            d->setWidgetHolder(ui->videoPreviewWidget, ui->wideHolder);
            ui->wideHolder->setHidden(noPreview);
            ui->narrowHolder->setHidden(false); //< There is a spacer child item.
            break;

        default:
            NX_ASSERT(false); //< Should never happen.
            break;
    }
}

EventTile::Style EventTile::visualStyle() const
{
    return d->style;
}

void EventTile::setVisualStyle(Style value)
{
    if (d->style == value)
        return;

    d->style = value;
    d->updateResourceListStyle();
    d->updatePalette();
}

bool EventTile::highlighted() const
{
    return d->highlighted;
}

void EventTile::setHighlighted(bool value)
{
    if (d->highlighted == value)
        return;

    d->highlighted = value;
    d->updatePalette();
}

void EventTile::clear()
{
    setCloseable(false);
    setIconPath({});
    d->clearLabel(ui->nameLabel, d->titleLabelDescriptor);
    setTitleColor({});
    d->clearLabel(ui->descriptionLabel, d->descriptionLabelDescriptor);
    setAttributeList({});
    setFooterText({});
    setTimestamp({});
    setPlaceholder({});
    setImageProvider({}, true);
    setVideoPreviewResource({});
    setPreviewHighlightRect({});
    setAction({});
    setBusyIndicatorVisible(false);
    setProgressBarVisible(false);
    setProgressValue(0.0);
    setProgressTitle({});
    setProgressFormat(QString());
    d->clearLabel(ui->resourceListLabel, d->resourceLabelDescriptor);
    setToolTip({});
    setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
}

} // namespace nx::vms::client::desktop
