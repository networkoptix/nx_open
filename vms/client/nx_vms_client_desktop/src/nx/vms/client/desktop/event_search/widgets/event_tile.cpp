#include "event_tile.h"
#include "ui_event_tile.h"

#include <QtCore/QScopedPointer>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QPainter>
#include <QtGui/QDesktopServices>

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <ui/common/palette.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/elided_label.h>
#include <utils/common/html.h>

#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/close_button.h>
#include <nx/vms/client/desktop/image_providers/resource_thumbnail_provider.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/workbench/extensions/workbench_progress_manager.h>
#include <nx/utils/log/log_message.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

namespace {

// Delay after which preview is initially requested.
static constexpr milliseconds kPreviewLoadDelay = 100ms;

// Delay after which preview is requested again in case of receiving "NO DATA".
static const milliseconds kPreviewReloadDelay = seconds(ini().rightPanelPreviewReloadDelay);

static constexpr auto kRoundingRadius = 2;

static constexpr int kTitleFontPixelSize = 13;
static constexpr int kTimestampFontPixelSize = 11;
static constexpr int kDescriptionFontPixelSize = 11;
static constexpr int kResourceListFontPixelSize = 11;
static constexpr int kAndMoreFontPixelSize = 11; //< "...and n more"
static constexpr int kFooterFontPixelSize = 11;

static constexpr int kTitleFontWeight = QFont::Medium;
static constexpr int kTimestampFontWeight = QFont::Normal;
static constexpr int kDescriptionFontWeight = QFont::Normal;
static constexpr int kResourceListFontWeight = QFont::Medium;
static constexpr int kAndMoreFontWeight = QFont::Normal; //< "...and n more"
static constexpr int kFooterFontWeight = QFont::Normal;

static constexpr int kAndMoreTopMargin = 4; //< Above "...and n more"

static constexpr int kProgressBarResolution = 1000;

static constexpr int kSeparatorHeight = 6;

static constexpr int kMaximumResourceListSize = 3; //< Before "...and n more"

static constexpr int kMaximumPreviewHeightWithHeader = 120;
static constexpr int kMaximumPreviewHeightWithoutHeader = 136;

static constexpr QMargins kMarginsWithHeader(8, 8, 8, 8);
static constexpr QMargins kMarginsWithoutHeader(8, 4, 8, 8);

static constexpr QMargins kWidePreviewMarginsWithHeader(0, 0, 0, 0);
static constexpr QMargins kWidePreviewMarginsWithoutHeader(0, 4, 0, 0);

static constexpr QMargins kNarrowPreviewMarginsWithHeader(0, 2, 0, 0);
static constexpr QMargins kNarrowPreviewMarginsWithoutHeader(0, 0, 0, 0);

// Close button margins are fine-tuned in correspondence with tile layout.
static constexpr QMargins kCloseButtonMarginsWithHeader(0, 6, 2, 0);
static constexpr QMargins kCloseButtonMarginsWithoutHeader(0, 2, 2, 0);

void setWidgetHolder(QWidget* widget, QWidget* newHolder)
{
    auto oldHolder = widget->parentWidget();
    const bool wasHidden = widget->isHidden();
    oldHolder->layout()->removeWidget(widget);
    widget->setParent(newHolder);
    newHolder->layout()->addWidget(widget);
    widget->setHidden(wasHidden);
}

milliseconds previewLoadDelay()
{
    return ini().tilePreviewLoadDelayOverrideMs > 0
        ? milliseconds(ini().tilePreviewLoadDelayOverrideMs)
        : kPreviewLoadDelay;
}

} // namespace

// ------------------------------------------------------------------------------------------------
// EventTile::Private

struct EventTile::Private
{
    EventTile* const q;
    CloseButton* const closeButton;
    WidgetAnchor* const closeButtonAnchor;
    bool closeable = false;
    CommandActionPtr action; //< Button action.
    QnElidedLabel* const progressLabel;
    const QScopedPointer<QTimer> loadPreviewTimer;
    bool automaticPreviewLoad = true;
    bool isPreviewLoadNeeded = false;
    bool forceNextPreviewUpdate = false;
    qreal progressValue = 0.0;
    bool isRead = false;
    bool footerEnabled = true;
    Style style = Style::standard;
    bool highlighted = false;
    QPalette defaultTitlePalette;
    Qt::MouseButton clickButton = Qt::NoButton;
    Qt::KeyboardModifiers clickModifiers;
    QPoint clickPoint;

    Private(EventTile* q):
        q(q),
        closeButton(new CloseButton(q)),
        closeButtonAnchor(anchorWidgetToParent(closeButton, Qt::RightEdge | Qt::TopEdge)),
        progressLabel(new QnElidedLabel(q)),
        loadPreviewTimer(new QTimer(q))
    {
        loadPreviewTimer->setSingleShot(true);
        QObject::connect(loadPreviewTimer.get(), &QTimer::timeout, [this]() { requestPreview(); });
    }

    void handleHoverChanged(bool hovered)
    {
        const auto showCloseButton = hovered && closeable;
        q->ui->timestampLabel->setHidden(showCloseButton || q->ui->timestampLabel->text().isEmpty());
        closeButton->setVisible(showCloseButton);
        updateBackgroundRole(hovered);

        if (showCloseButton)
            closeButton->raise();
    }

    void updateBackgroundRole(bool hovered)
    {
        q->setBackgroundRole(hovered ? QPalette::Midlight : QPalette::Window);
    }

    void updatePalette()
    {
        auto pal = q->palette();
        const auto base = pal.color(QPalette::Base);

        int lighterBy = highlighted ? 2 : 0;
        if (style == Style::informer)
            ++lighterBy;

        pal.setColor(QPalette::Window, colorTheme()->lighter(base, lighterBy));
        pal.setColor(QPalette::Midlight, colorTheme()->lighter(base, lighterBy + 1));
        q->setPalette(pal);
    }

    void setResourceList(const QStringList& list, int andMore)
    {
        if (list.empty())
        {
            q->ui->resourceListHolder->hide();
            q->ui->resourceListLabel->setText({});
        }
        else
        {
            QString text = list.join("<br>");
            if (andMore > 0)
            {
                static constexpr int kQssFontWeightMultiplier = 8;

                text += lm("<p style='color: %1; font-size: %2px; font-weight: %3; margin-top: %4'>%5</p>")
                    .args(
                        q->palette().color(QPalette::WindowText).name(),
                        kAndMoreFontPixelSize,
                        kAndMoreFontWeight * kQssFontWeightMultiplier,
                        kAndMoreTopMargin,
                        tr("...and %n more", "", andMore));
            }

            q->ui->resourceListLabel->setText(text);
            q->ui->resourceListHolder->show();
        }
    }

    bool isPreviewNeeded() const
    {
        return q->preview() && q->previewEnabled();
    }

    bool isPreviewUpdateRequired() const
    {
        if (!isPreviewNeeded() || !NX_ASSERT(q->preview()))
            return false;

        if (forceNextPreviewUpdate)
            return true;

        switch (q->preview()->status())
        {
            case Qn::ThumbnailStatus::Invalid:
            case Qn::ThumbnailStatus::NoData:
                return true;

            default:
                return false;
        }
    }

    void requestPreview()
    {
        if (!isPreviewUpdateRequired())
            return;

        NX_VERBOSE(this, "Requesting tile preview");
        forceNextPreviewUpdate = false;

        if (automaticPreviewLoad)
        {
            q->preview()->loadAsync();
        }
        else
        {
            isPreviewLoadNeeded = true;
            emit q->needsPreviewLoad();
        }
    }

    void updatePreview(milliseconds delay)
    {
        if (isPreviewUpdateRequired())
            loadPreviewTimer->start(delay);
        else
            loadPreviewTimer->stop();
    }

    void showDebugPreviewTimestamp()
    {
        auto provider = qobject_cast<ResourceThumbnailProvider*>(q->preview());
        if (provider)
        {
            q->ui->debugPreviewTimeLabel->setText(
                lm("Preview: %2 us").arg(provider->timestamp().count()));
            q->ui->debugPreviewTimeLabel->setVisible(
                provider->status() == Qn::ThumbnailStatus::Loaded);
        }
        else
        {
            q->ui->debugPreviewTimeLabel->hide();
            q->ui->debugPreviewTimeLabel->setText({});
        }
    };
};

// ------------------------------------------------------------------------------------------------
// EventTile

EventTile::EventTile(QWidget* parent):
    base_type(parent, Qt::FramelessWindowHint),
    d(new Private(this)),
    ui(new Ui::EventTile())
{
    ui->setupUi(this);
    setAttribute(Qt::WA_Hover);

    d->closeButton->setHidden(true);
    d->closeButtonAnchor->setMargins(kCloseButtonMarginsWithHeader);

    ui->mainWidget->layout()->setContentsMargins(kMarginsWithHeader);
    ui->wideHolder->layout()->setContentsMargins(kWidePreviewMarginsWithHeader);
    ui->narrowHolder->layout()->setContentsMargins(kNarrowPreviewMarginsWithHeader);
    ui->previewWidget->setMaximumHeight(kMaximumPreviewHeightWithHeader);

    auto sizePolicy = ui->timestampLabel->sizePolicy();
    sizePolicy.setRetainSizeWhenHidden(true);
    ui->timestampLabel->setSizePolicy(sizePolicy);

    ui->descriptionLabel->hide();
    ui->debugPreviewTimeLabel->hide();
    ui->timestampLabel->hide();
    ui->actionHolder->hide();
    ui->footerLabel->hide();
    ui->resourceListHolder->hide();
    ui->progressDescriptionLabel->hide();
    ui->narrowHolder->hide();
    ui->wideHolder->hide();

    ui->previewWidget->setAutoScaleDown(false);
    ui->previewWidget->setCropMode(AsyncImageWidget::CropMode::notHovered);
    ui->previewWidget->setReloadMode(AsyncImageWidget::ReloadMode::showPreviousImage);

    ui->nameLabel->setForegroundRole(QPalette::Light);
    ui->timestampLabel->setForegroundRole(QPalette::WindowText);
    ui->descriptionLabel->setForegroundRole(QPalette::Light);
    ui->debugPreviewTimeLabel->setForegroundRole(QPalette::Light);
    ui->resourceListLabel->setForegroundRole(QPalette::Light);
    ui->footerLabel->setForegroundRole(QPalette::Light);

    QFont font;
    font.setWeight(kTitleFontWeight);
    font.setPixelSize(kTitleFontPixelSize);
    ui->nameLabel->setFont(font);
    ui->nameLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->nameLabel->setOpenExternalLinks(false);

    font.setWeight(kTimestampFontWeight);
    font.setPixelSize(kTimestampFontPixelSize);
    ui->timestampLabel->setFont(font);
    ui->timestampLabel->setProperty(style::Properties::kDontPolishFontProperty, true);

    font.setWeight(kDescriptionFontWeight);
    font.setPixelSize(kDescriptionFontPixelSize);
    ui->descriptionLabel->setFont(font);
    ui->descriptionLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->descriptionLabel->setOpenExternalLinks(false);
    ui->progressDescriptionLabel->setFont(font);
    ui->progressDescriptionLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->progressDescriptionLabel->setOpenExternalLinks(false);
    ui->debugPreviewTimeLabel->setFont(font);
    ui->debugPreviewTimeLabel->setProperty(style::Properties::kDontPolishFontProperty, true);

    font.setWeight(kResourceListFontWeight);
    font.setPixelSize(kResourceListFontPixelSize);
    ui->resourceListLabel->setFont(font);
    ui->resourceListLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->resourceListLabel->setOpenExternalLinks(false);

    font.setWeight(kFooterFontWeight);
    font.setPixelSize(kFooterFontPixelSize);
    ui->footerLabel->setFont(font);
    ui->footerLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->footerLabel->setOpenExternalLinks(false);

    ui->busyIndicator->setContentsMargins(
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding);

    ui->busyIndicator->hide();
    ui->progressHolder->hide();
    ui->mainWidget->hide();

    ui->secondaryTimestampHolder->hide();

    QFont progressLabelFont;
    progressLabelFont.setWeight(QFont::DemiBold);

    d->progressLabel->setParent(ui->progressBar);
    d->progressLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    d->progressLabel->setFont(progressLabelFont);
    d->progressLabel->setForegroundRole(QPalette::Highlight);

    ui->nameLabel->setText({});
    ui->descriptionLabel->setText({});
    ui->footerLabel->setText({});
    ui->timestampLabel->setText({});

    static constexpr int kProgressLabelShift = 8;
    anchorWidgetToParent(d->progressLabel, {0, 0, 0, kProgressLabelShift});

    ui->progressBar->setRange(0, kProgressBarResolution);
    ui->progressBar->setValue(0);

    ui->nameLabel->ensurePolished();
    d->defaultTitlePalette = ui->nameLabel->palette();

    connect(d->closeButton, &QPushButton::clicked, this, &EventTile::closeRequested);

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
    connect(ui->footerLabel, &QLabel::linkActivated, this, activateLink);
}

EventTile::EventTile(
    const QString& title,
    const QPixmap& icon,
    const QString& timestamp,
    const QString& description,
    QWidget* parent)
    :
    EventTile(parent)
{
    setTitle(title);
    setIcon(icon);
    setTimestamp(timestamp);
    setDescription(description);
}

EventTile::~EventTile()
{
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
    d->handleHoverChanged(underMouse());
}

QString EventTile::title() const
{
    return ui->nameLabel->text();
}

void EventTile::setTitle(const QString& value)
{
    ui->nameLabel->setText(value);
    ui->mainWidget->setHidden(value.isEmpty());
}

QColor EventTile::titleColor() const
{
    ui->nameLabel->ensurePolished();
    return ui->nameLabel->palette().color(ui->nameLabel->foregroundRole());
}

void EventTile::setTitleColor(const QColor& value)
{
    if (value.isValid())
        setPaletteColor(ui->nameLabel, ui->nameLabel->foregroundRole(), value);
    else
        ui->nameLabel->setPalette(d->defaultTitlePalette);
}

QString EventTile::description() const
{
    return ui->descriptionLabel->text();
}

void EventTile::setDescription(const QString& value)
{
    ui->descriptionLabel->setText(value);
    ui->descriptionLabel->setHidden(value.isEmpty());
    ui->progressDescriptionLabel->setText(value);
    ui->progressDescriptionLabel->setHidden(value.isEmpty());
}

void EventTile::setResourceList(const QnResourceList& list)
{
    QStringList items;
    for (int i = 0; i < std::min(list.size(), kMaximumResourceListSize); ++i)
    {
        NX_ASSERT(list[i]); //< Null resource pointer is an abnormal situation.
        items.push_back(list[i] ? htmlBold(list[i]->getName()) : "?");
    }

    d->setResourceList(items, qMax(list.size() - kMaximumResourceListSize, 0));
}

void EventTile::setResourceList(const QStringList& list)
{
    QStringList items = list.mid(0, kMaximumResourceListSize);
    for (auto& item: items)
        item = ensureHtml(item);

    d->setResourceList(items, qMax(list.size() - kMaximumResourceListSize, 0));
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

QString EventTile::timestamp() const
{
    return ui->timestampLabel->text();
}

void EventTile::setTimestamp(const QString& value)
{
    ui->timestampLabel->setText(value);
    ui->timestampLabel->setHidden(value.isEmpty() || !d->closeButton->isHidden());
}

QPixmap EventTile::icon() const
{
    if (const auto pixmapPtr = ui->iconLabel->pixmap())
        return *pixmapPtr;

    return QPixmap();
}

void EventTile::setIcon(const QPixmap& value)
{
    // TODO: #vkutin Do we want to scale them? Now it's a temporary measure for soft triggers.
    ui->iconLabel->setPixmap(value.isNull() ? value : value.scaled(
        ui->iconLabel->maximumSize() * value.devicePixelRatio(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation));
    // Icon label is always visible. It keeps column width fixed.
}

ImageProvider* EventTile::preview() const
{
    return ui->previewWidget->imageProvider();
}

void EventTile::setPreview(ImageProvider* value, bool forceUpdate)
{
    if (preview() == value && !forceUpdate)
        return;

    if (preview())
        preview()->disconnect(this);

    ui->previewWidget->setImageProvider(value);
    ui->previewWidget->parentWidget()->setHidden(!value);

    d->isPreviewLoadNeeded = false;
    d->forceNextPreviewUpdate = true;
    d->updatePreview(previewLoadDelay());

    if (ini().showDebugTimeInformationInRibbon)
        d->showDebugPreviewTimestamp();

    if (!preview())
        return;

    connect(preview(), &ImageProvider::statusChanged, this,
        [this](Qn::ThumbnailStatus status)
        {
            if (status != Qn::ThumbnailStatus::Invalid)
                d->isPreviewLoadNeeded = false;

            if (status == Qn::ThumbnailStatus::NoData && kPreviewReloadDelay > 0s)
                d->updatePreview(kPreviewReloadDelay);

            if (ini().showDebugTimeInformationInRibbon)
                d->showDebugPreviewTimestamp();
        });
}

QRectF EventTile::previewCropRect() const
{
    return ui->previewWidget->highlightRect();
}

void EventTile::setPreviewCropRect(const QRectF& relativeRect)
{
    ui->previewWidget->setHighlightRect(relativeRect);
}

bool EventTile::automaticPreviewLoad() const
{
    return d->automaticPreviewLoad;
}

void EventTile::setAutomaticPreviewLoad(bool value)
{
    if (d->automaticPreviewLoad == value)
        return;

    d->automaticPreviewLoad = value;
    d->isPreviewLoadNeeded = d->isPreviewLoadNeeded && !d->automaticPreviewLoad;
    d->updatePreview(previewLoadDelay());
}

bool EventTile::isPreviewLoadNeeded() const
{
    return d->isPreviewNeeded() && d->isPreviewLoadNeeded;
}

CommandActionPtr EventTile::action() const
{
    return d->action;
}

void EventTile::setAction(const CommandActionPtr& value)
{
    d->action = value;
    ui->actionButton->setAction(d->action.data());
    ui->actionHolder->setHidden(d->action.isNull());
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
            d->handleHoverChanged(true);
            break;

        case QEvent::Leave:
        case QEvent::HoverLeave:
        case QEvent::Hide:
            d->handleHoverChanged(false);
            break;

        case QEvent::Show:
            d->handleHoverChanged(underMouse());
            break;

        case QEvent::MouseButtonPress:
        {
            const auto mouseEvent = static_cast<QMouseEvent*>(event);
            base_type::event(event);
            event->accept();
            d->clickButton = mouseEvent->button();
            d->clickModifiers = mouseEvent->modifiers();
            d->clickPoint = mouseEvent->pos();
            return true;
        }

        case QEvent::MouseButtonRelease:
        {
            const auto mouseEvent = static_cast<QMouseEvent*>(event);
            if ((mouseEvent->button() == d->clickButton) && closeToStart(mouseEvent->pos()))
                emit clicked(d->clickButton, mouseEvent->modifiers() & d->clickModifiers);
            d->clickButton = Qt::NoButton;
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

qreal EventTile::progressValue() const
{
    return d->progressValue;
}

void EventTile::setProgressValue(qreal value)
{
    if (qFuzzyIsNull(d->progressValue - value))
        return;

    d->progressValue = value;

    if (d->progressValue != WorkbenchProgressManager::kIndefiniteProgressValue)
    {
        // Finite progress.
        ui->progressBar->setRange(0, kProgressBarResolution);
        ui->progressBar->setValue(int(value * kProgressBarResolution));
    }
    else
    {
        // Ininite progress.
        ui->progressBar->setRange(0, 0);
        ui->progressBar->setValue(0);
    }
}

QString EventTile::progressTitle() const
{
    return d->progressLabel->text();
}

void EventTile::setProgressTitle(const QString& value)
{
    d->progressLabel->setText(value);
}

bool EventTile::previewEnabled() const
{
    return !ui->previewWidget->isHidden();
}

void EventTile::setPreviewEnabled(bool value)
{
    if (previewEnabled() == value)
        return;

    ui->previewWidget->setHidden(!value);
    ui->previewWidget->parentWidget()->setHidden(!value || !ui->previewWidget->imageProvider());

    d->updatePreview(previewLoadDelay());
}

bool EventTile::footerEnabled() const
{
    return d->footerEnabled;
}

void EventTile::setFooterEnabled(bool value)
{
    d->footerEnabled = value;
    ui->footerLabel->setHidden(!d->footerEnabled || ui->footerLabel->text().isEmpty());
}

bool EventTile::headerEnabled() const
{
    return !ui->iconLabel->isHidden();
}

void EventTile::setHeaderEnabled(bool value)
{
    if (headerEnabled() == value)
        return;

    ui->iconLabel->setHidden(!value);
    ui->nameLabel->setHidden(!value);

    if (value)
    {
        setWidgetHolder(ui->timestampLabel, ui->primaryTimestampHolder);
        ui->secondaryTimestampHolder->hide();
        ui->primaryTimestampHolder->show();
        ui->previewWidget->setMaximumHeight(kMaximumPreviewHeightWithHeader);
        ui->mainWidget->layout()->setContentsMargins(kMarginsWithHeader);
        ui->wideHolder->layout()->setContentsMargins(kWidePreviewMarginsWithHeader);
        ui->narrowHolder->layout()->setContentsMargins(kNarrowPreviewMarginsWithHeader);
        d->closeButtonAnchor->setMargins(kCloseButtonMarginsWithHeader);
    }
    else
    {
        setWidgetHolder(ui->timestampLabel, ui->secondaryTimestampHolder);
        ui->secondaryTimestampHolder->show();
        ui->primaryTimestampHolder->hide();
        ui->previewWidget->setMaximumHeight(kMaximumPreviewHeightWithoutHeader);
        ui->mainWidget->layout()->setContentsMargins(kMarginsWithoutHeader);
        ui->wideHolder->layout()->setContentsMargins(kWidePreviewMarginsWithoutHeader);
        ui->narrowHolder->layout()->setContentsMargins(kNarrowPreviewMarginsWithoutHeader);
        d->closeButtonAnchor->setMargins(kCloseButtonMarginsWithoutHeader);
    }
}

EventTile::Mode EventTile::mode() const
{
    if (ui->previewWidget->parentWidget() == ui->narrowHolder)
        return Mode::standard;

    NX_ASSERT(ui->previewWidget->parentWidget() == ui->wideHolder);
    return Mode::wide;
}

void EventTile::setMode(Mode value)
{
    if (mode() == value)
        return;

    const bool noPreview = ui->previewWidget->isHidden() || !ui->previewWidget->imageProvider();
    switch (value)
    {
        case Mode::standard:
            setWidgetHolder(ui->previewWidget, ui->narrowHolder);
            ui->narrowHolder->setHidden(noPreview);
            ui->wideHolder->setHidden(true);
            break;

        case Mode::wide:
            setWidgetHolder(ui->previewWidget, ui->wideHolder);
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
    setTitle({});
    setTitleColor({});
    setDescription({});
    setFooterText({});
    setTimestamp({});
    setIcon({});
    setPreview({}, false);
    setPreviewCropRect({});
    setAction({});
    setBusyIndicatorVisible(false);
    setProgressBarVisible(false);
    setProgressValue(0.0);
    setProgressTitle({});
    setResourceList(QStringList());
    setToolTip({});
    setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
}

} // namespace nx::vms::client::desktop
