// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_tile.h"
#include "ui_event_tile.h"

#include <QtCore/QCache>
#include <QtCore/QScopedPointer>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtGui/QPainter>
#include <QtGui/QTextDocument>

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <finders/systems_finder.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/close_button.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/image_providers/resource_thumbnail_provider.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/palette.h>
#include <ui/widgets/common/elided_label.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::common::html;
using namespace std::chrono;

namespace {

// Delay after which preview is requested again in case of receiving "NO DATA".
static const milliseconds kPreviewReloadDelay = seconds(ini().rightPanelPreviewReloadDelay);

static constexpr auto kRoundingRadius = 2;
static constexpr int kTileTitleLineLimit = 2;

static constexpr auto kTitleFontWeight = QFont::Medium;
static constexpr auto kTimestampFontWeight = QFont::Normal;
static constexpr auto kDescriptionFontWeight = QFont::Normal;
static constexpr auto kResourceListFontWeight = QFont::Medium;
static constexpr auto kAndMoreFontWeight = QFont::Normal; //< "...and n more"
static constexpr auto kFooterFontWeight = QFont::Normal;

static constexpr int kAndMoreTopMargin = 4; //< Above "...and n more"

static constexpr int kProgressBarResolution = 1000;

static constexpr int kSeparatorHeight = 6;

static constexpr qsizetype kMaximumResourceListSize = 3; //< Before "...and n more"

static constexpr int kMaximumPreviewHeightWithHeader = 135;
static constexpr int kMaximumPreviewHeightWithoutHeader = 151;

static constexpr QMargins kMarginsWithHeader(8, 8, 8, 8);
static constexpr QMargins kMarginsWithoutHeader(8, 4, 8, 8);

static constexpr QMargins kWidePreviewMarginsWithHeader(0, 0, 0, 0);
static constexpr QMargins kWidePreviewMarginsWithoutHeader(0, 4, 0, 0);

static constexpr QMargins kNarrowPreviewMarginsWithHeader(0, 2, 0, 0);
static constexpr QMargins kNarrowPreviewMarginsWithoutHeader(0, 0, 0, 0);

// Close button margins are fine-tuned in correspondence with tile layout.
static constexpr QMargins kCloseButtonMarginsWithHeader(0, 6, 2, 0);
static constexpr QMargins kCloseButtonMarginsWithoutHeader(0, 2, 2, 0);

static constexpr auto kDefaultReloadMode = AsyncImageWidget::ReloadMode::showPreviousImage;

constexpr auto kDotRadius = 8;
constexpr auto kDotSpacing = 4;

void setWidgetHolder(QWidget* widget, QWidget* newHolder)
{
    auto oldHolder = widget->parentWidget();
    const bool wasHidden = widget->isHidden();
    oldHolder->layout()->removeWidget(widget);
    widget->setParent(newHolder);
    newHolder->layout()->addWidget(widget);
    widget->setHidden(wasHidden);
}

QString getSystemName(const QString& systemId)
{
    if (systemId.isEmpty())
        return QString();

    auto systemDescription = qnSystemsFinder->getSystem(systemId);
    if (!NX_ASSERT(systemDescription))
        return QString();

    return systemDescription->name();
}

// For cloud notifications the system name must be displayed. The function returns the name of
// a system different from the current one in the form required for display.
QString getFormattedSystemNameIfNeeded(const QString& systemId)
{
    if (auto systemName = getSystemName(systemId); !systemName.isEmpty())
        return systemName + " / ";

    return QString();
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
    CommandActionPtr additionalAction;
    QnElidedLabel* const progressLabel;
    const QScopedPointer<QTimer> loadPreviewTimer;
    bool automaticPreviewLoad = true;
    bool isPreviewLoadNeeded = false;
    bool forceNextPreviewUpdate = false;
    std::optional<qreal> progressValue = 0.0;
    bool isRead = false;
    bool footerEnabled = true;
    Style style = Style::standard;
    bool highlighted = false;
    QPalette defaultTitlePalette;
    Qt::MouseButton clickButton = Qt::NoButton;
    Qt::KeyboardModifiers clickModifiers;
    QPoint clickPoint;
    QString title;
    QCache<int, QString> titleByWidth; //< key - width of nameLabel, value - trimmed title string
    int currentWidth = 0;

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

    void setTitle(const QString& value)
    {
        if (title == value)
            return;

        title = value;
        currentWidth = 0;
        titleByWidth.clear();
        titleByWidth.insert(0, new QString(value));

        updateTitleForCurrentWidth();
    }

    void updateTitleForCurrentWidth()
    {
        const auto width = q->ui->nameLabel->width();
        if (width == 0 || width == currentWidth)
            return;

        QString text;
        if (const auto textPtr = titleByWidth.object(width))
        {
            text = *textPtr;
        }
        else
        {
            QTextDocument doc;
            doc.setHtml(toHtml(title));
            doc.setTextWidth(width);
            WidgetUtils::elideDocumentLines(&doc, kTileTitleLineLimit, true);
            text = doc.toHtml();
            titleByWidth.insert(width, new QString(text));
        }

        q->ui->nameLabel->setText(text);
        currentWidth = width;
    }

    void handleHoverChanged(bool hovered)
    {
        if (qApp->activePopupWidget() || qApp->activeModalWidget())
            return;
        const auto showCloseButton = (hovered || q->progressBarVisible()) && closeable;
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

        pal.setColor(QPalette::Window, core::colorTheme()->lighter(base, lighterBy));
        pal.setColor(QPalette::Midlight, core::colorTheme()->lighter(base, lighterBy + 1));
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
            QString text = list.join(kLineBreak);
            if (andMore > 0)
            {
                text += nx::format("<p style='color: %1; font-size: %2px; font-weight: %3; margin-top: %4'>%5</p>")
                    .args(
                        q->palette().color(QPalette::WindowText).name(),
                        fontConfig()->small().pixelSize(),
                        (int) kAndMoreFontWeight,
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

    void updatePreview(milliseconds delay = 0ms)
    {
        if (delay <= 0ms)
        {
            // Still must be delayed.
            if (isPreviewUpdateRequired())
                executeLater([this]() { requestPreview(); }, q);
        }
        else
        {
            if (isPreviewUpdateRequired())
                loadPreviewTimer->start(delay);
            else
                loadPreviewTimer->stop();
        }
    }

    void showDebugPreviewTimestamp()
    {
        auto provider = qobject_cast<ResourceThumbnailProvider*>(q->preview());
        if (provider)
        {
            q->ui->debugPreviewTimeLabel->setText(
                nx::format("Preview: %2 us").arg(provider->timestamp().count()));
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

    setPaletteColor(this, QPalette::Base, core::colorTheme()->color("dark7"));
    setPaletteColor(this, QPalette::Light, core::colorTheme()->color("light10"));
    setPaletteColor(this, QPalette::WindowText, core::colorTheme()->color("light16"));
    setPaletteColor(this, QPalette::Text, core::colorTheme()->color("light4"));
    setPaletteColor(this, QPalette::Highlight, core::colorTheme()->color("brand_core"));

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
    ui->attributeTable->hide();
    ui->footerLabel->hide();
    ui->resourceListHolder->hide();
    ui->progressDescriptionLabel->hide();
    ui->narrowHolder->hide();
    ui->wideHolder->hide();

    auto dots = ui->previewWidget->busyIndicator()->dots();
    dots->setDotRadius(kDotRadius);
    dots->setDotSpacing(kDotSpacing);

    ui->previewWidget->setAutoScaleUp(true);
    ui->previewWidget->setReloadMode(kDefaultReloadMode);
    ui->previewWidget->setCropMode(AsyncImageWidget::CropMode::always);

    ui->nameLabel->setForegroundRole(QPalette::Light);
    ui->timestampLabel->setForegroundRole(QPalette::WindowText);
    ui->descriptionLabel->setForegroundRole(QPalette::Light);
    ui->debugPreviewTimeLabel->setForegroundRole(QPalette::Light);
    ui->resourceListLabel->setForegroundRole(QPalette::Light);
    ui->footerLabel->setForegroundRole(QPalette::Light);

    QFont font;
    font.setWeight(kTitleFontWeight);
    font.setPixelSize(fontConfig()->normal().pixelSize());
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
    font.setPixelSize(fontConfig()->small().pixelSize());
    ui->resourceListLabel->setFont(font);
    ui->resourceListLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->resourceListLabel->setOpenExternalLinks(false);

    font.setWeight(kFooterFontWeight);
    font.setPixelSize(fontConfig()->small().pixelSize());
    ui->attributeTable->setFont(font);
    ui->attributeTable->setProperty(style::Properties::kDontPolishFontProperty, true);
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

    ui->secondaryTimestampHolder->hide();

    QFont progressLabelFont;
    progressLabelFont.setWeight(QFont::Medium);

    d->progressLabel->setParent(ui->progressBar);
    d->progressLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    d->progressLabel->setFont(progressLabelFont);
    d->progressLabel->setForegroundRole(QPalette::Highlight);

    ui->nameLabel->setText({});
    ui->descriptionLabel->setText({});
    ui->attributeTable->setContent({});
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

    ui->nameLabel->installEventFilter(this);
}

EventTile::~EventTile()
{
}

bool EventTile::eventFilter(QObject* object, QEvent* event)
{
    if (object == ui->nameLabel && event->type() == QEvent::Resize)
        d->updateTitleForCurrentWidth();

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

    d->handleHoverChanged(underMouse());
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

void EventTile::setResourceList(const QnResourceList& list, const QString& cloudSystemId)
{
    QStringList items;
    auto systemName = getFormattedSystemNameIfNeeded(cloudSystemId);
    for (int i = 0; i < std::min(list.size(), kMaximumResourceListSize); ++i)
    {
        NX_ASSERT(list[i]); //< Null resource pointer is an abnormal situation.
        items.push_back(list[i]
            ? bold(toHtmlEscaped(systemName + list[i]->getName()))
            : toHtmlEscaped(systemName) + "?");
    }

    if (items.isEmpty() && !cloudSystemId.isEmpty())
        items.push_back(bold(getSystemName(cloudSystemId)));

    d->setResourceList(items, qMax(list.size() - kMaximumResourceListSize, 0));
}

void EventTile::setResourceList(const QStringList& list, const QString& cloudSystemId)
{
    auto systemName = getFormattedSystemNameIfNeeded(cloudSystemId);
    QStringList items = list.mid(0, kMaximumResourceListSize);
    for (auto& item: items)
        item = toHtml(systemName + item);

    if (items.isEmpty() && !cloudSystemId.isEmpty())
        items.push_back(bold(getSystemName(cloudSystemId)));

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
    ui->timestampLabel->setHidden(value.isEmpty() || !d->closeButton->isHidden());
}

QPixmap EventTile::icon() const
{
    return ui->iconLabel->pixmap();
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
    ui->previewWidget->parentWidget()->setVisible(
        value || !ui->previewWidget->placeholder().isEmpty());

    d->isPreviewLoadNeeded = false;
    d->forceNextPreviewUpdate = forceUpdate;
    d->updatePreview();

    if (core::ini().showDebugTimeInformationInEventSearchData)
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

            if (core::ini().showDebugTimeInformationInEventSearchData)
                d->showDebugPreviewTimestamp();
        });
}

void EventTile::setPlaceholder(const QString& text)
{
    ui->previewWidget->setPlaceholder(text);

    if (!text.isEmpty())
       setPreview(nullptr, /*forceUpdate*/ true);
}

void EventTile::setForcePreviewLoader(bool force)
{
    ui->previewWidget->setReloadMode(force
        ? AsyncImageWidget::ReloadMode::showLoadingIndicator
        : kDefaultReloadMode);

    ui->previewWidget->setForceLoadingIndicator(force);
}

QRectF EventTile::previewHighlightRect() const
{
    return ui->previewWidget->highlightRect();
}

void EventTile::setPreviewHighlightRect(const QRectF& relativeRect)
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
    d->updatePreview();
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

CommandActionPtr EventTile::additionalAction() const
{
    return d->additionalAction;
}

void EventTile::setAdditionalAction(const CommandActionPtr& value)
{
    d->additionalAction = value;
    ui->additionalActionButton->setAction(d->additionalAction.data());
    ui->additionalActionButton->setHidden(d->additionalAction.isNull());
    ui->additionalActionButton->setFlat(true);
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
    return !ui->previewWidget->isHidden();
}

void EventTile::setPreviewEnabled(bool value)
{
    if (previewEnabled() == value)
        return;

    ui->previewWidget->setHidden(!value);
    ui->previewWidget->parentWidget()->setHidden(!value || !ui->previewWidget->imageProvider());

    d->updatePreview();
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
    setAttributeList({});
    setFooterText({});
    setTimestamp({});
    setIcon({});
    setPlaceholder({});
    setPreview({}, true);
    setPreviewHighlightRect({});
    setAction({});
    setBusyIndicatorVisible(false);
    setProgressBarVisible(false);
    setProgressValue(0.0);
    setProgressTitle({});
    setProgressFormat(QString());
    setResourceList(QStringList(), QString());
    setToolTip({});
    setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
}

} // namespace nx::vms::client::desktop
