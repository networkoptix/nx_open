#include "event_tile.h"
#include "ui_event_tile.h"

#include <QtCore/QUrl>
#include <QtCore/QTimer>

#include <QtGui/QPainter>
#include <QtGui/QDesktopServices>

#include <client/client_globals.h>
#include <ui/common/palette.h>
#include <ui/common/widget_anchor.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/elided_label.h>
#include <utils/common/delayed.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr auto kRoundingRadius = 2;

static constexpr int kTitleFontPixelSize = 13;
static constexpr int kTimestampFontPixelSize = 11;
static constexpr int kDescriptionFontPixelSize = 11;

static constexpr int kTitleFontWeight = QFont::Medium;
static constexpr int kTimestampFontWeight = QFont::Normal;
static constexpr int kDescriptionFontWeight = QFont::Normal;

static constexpr int kProgressBarResolution = 1000;

} // namespace

EventTile::EventTile(QWidget* parent):
    base_type(parent, Qt::FramelessWindowHint),
    ui(new Ui::EventTile()),
    m_closeButton(new QPushButton(this)),
    m_progressLabel(new QnElidedLabel(this))
{
    ui->setupUi(this);
    setAttribute(Qt::WA_Hover);

    m_closeButton->setIcon(qnSkin->icon(lit("events/notification_close.png")));
    m_closeButton->setIconSize(QnSkin::maximumSize(m_closeButton->icon()));
    m_closeButton->setFixedSize(m_closeButton->iconSize());
    m_closeButton->setFlat(true);
    m_closeButton->setHidden(true);
    auto anchor = new QnWidgetAnchor(m_closeButton);
    anchor->setEdges(Qt::RightEdge | Qt::TopEdge);

    auto sizePolicy = ui->timestampLabel->sizePolicy();
    sizePolicy.setRetainSizeWhenHidden(true);
    ui->timestampLabel->setSizePolicy(sizePolicy);

    ui->descriptionLabel->setHidden(true);
    ui->timestampLabel->setHidden(true);
    ui->previewWidget->setHidden(true);

    ui->previewWidget->setCropMode(QnResourcePreviewWidget::CropMode::notHovered);

    ui->nameLabel->setForegroundRole(QPalette::Light);
    ui->timestampLabel->setForegroundRole(QPalette::WindowText);
    ui->descriptionLabel->setForegroundRole(QPalette::Light);

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

    ui->busyIndicator->setContentsMargins(
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding);

    ui->busyIndicator->setHidden(true);
    ui->progressHolder->setHidden(true);
    ui->mainWidget->setHidden(true);

    QFont progressLabelFont;
    progressLabelFont.setWeight(QFont::DemiBold);

    ui->progressBar->setRange(0, kProgressBarResolution);
    m_progressLabel->setParent(ui->progressBar);
    m_progressLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    m_progressLabel->setFont(progressLabelFont);
    m_progressLabel->setForegroundRole(QPalette::Highlight);

    static constexpr int kProgressLabelShift = 8;
    auto progressLabelAnchor = new QnWidgetAnchor(m_progressLabel);
    progressLabelAnchor->setMargins(0, 0, 0, kProgressLabelShift);

    connect(m_closeButton, &QPushButton::clicked, this, &EventTile::closeRequested);

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
    return m_closeable;
}

void EventTile::setCloseable(bool value)
{
    if (m_closeable == value)
        return;

    m_closeable = value;
    handleHoverChanged(m_closeable && underMouse());
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
    ui->nameLabel->ensurePolished();
    setPaletteColor(ui->nameLabel, ui->nameLabel->foregroundRole(), value);
}

QString EventTile::description() const
{
    return ui->descriptionLabel->text();
}

void EventTile::setDescription(const QString& value)
{
    ui->descriptionLabel->setText(value);
    ui->descriptionLabel->setHidden(value.isEmpty());
}

QString EventTile::timestamp() const
{
    return ui->timestampLabel->text();
}

void EventTile::setTimestamp(const QString& value)
{
    ui->timestampLabel->setText(value);
    ui->timestampLabel->setHidden(value.isEmpty() || !m_closeButton->isHidden());
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

QnImageProvider* EventTile::preview() const
{
    return ui->previewWidget->imageProvider();
}

void EventTile::setPreview(QnImageProvider* value)
{
    ui->previewWidget->setImageProvider(value);
    ui->previewWidget->setHidden(!value);
}

QRectF EventTile::previewCropRect() const
{
    return ui->previewWidget->highlightRect();
}

void EventTile::setPreviewCropRect(const QRectF& relativeRect)
{
    ui->previewWidget->setHighlightRect(relativeRect);
}

CommandActionPtr EventTile::action() const
{
    return m_action;
}

void EventTile::setAction(const CommandActionPtr& value)
{
    m_action = value;
    ui->actionButton->setAction(m_action.data());
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

bool EventTile::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::Enter:
        case QEvent::HoverEnter:
            handleHoverChanged(true);
            break;

        case QEvent::Leave:
        case QEvent::HoverLeave:
            handleHoverChanged(false);
            break;

        case QEvent::MouseMove:
        case QEvent::HoverMove:
            updateBackgroundRole(!m_closeButton->underMouse());
            break;

        case QEvent::MouseButtonPress:
            base_type::event(event);
            event->accept();
            return true;

        case QEvent::MouseButtonRelease:
            emit clicked();
            break;

        default:
            break;
    }

    return base_type::event(event);
}

void EventTile::handleHoverChanged(bool hovered)
{
    const auto showCloseButton = hovered & m_closeable;
    ui->timestampLabel->setHidden(showCloseButton || ui->timestampLabel->text().isEmpty());
    m_closeButton->setVisible(showCloseButton);
    updateBackgroundRole(hovered && !m_closeButton->underMouse());

    if (m_autoCloseTimer)
    {
        if (hovered)
            m_autoCloseTimer->stop();
        else
            m_autoCloseTimer->start();
    }
}

void EventTile::updateBackgroundRole(bool hovered)
{
    setBackgroundRole(hovered ? QPalette::Midlight : QPalette::Window);
}

bool EventTile::hasAutoClose() const
{
    return closeable() && m_autoCloseTimer;
}

int EventTile::autoCloseTimeMs() const
{
    return hasAutoClose() ? m_autoCloseTimer->interval() : -1;
}

int EventTile::autoCloseRemainingMs() const
{
    return hasAutoClose() ? m_autoCloseTimer->remainingTime() : -1;
}

void EventTile::setAutoCloseTimeMs(int value)
{
    if (value <= 0)
    {
        if (!m_autoCloseTimer)
            return;

        m_autoCloseTimer->deleteLater();
        m_autoCloseTimer = nullptr;
        return;
    }

    const auto autoClose =
        [this]()
        {
            if (closeable())
                emit closeRequested();
        };

    if (m_autoCloseTimer)
        m_autoCloseTimer->setInterval(value);
    else
        m_autoCloseTimer = executeDelayedParented(autoClose, value, this);
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
    return m_progressValue;
}

void EventTile::setProgressValue(qreal value)
{
    m_progressValue = value;
    ui->progressBar->setValue(int(value * kProgressBarResolution));
}

QString EventTile::progressTitle() const
{
    return m_progressLabel->text();
}

void EventTile::setProgressTitle(const QString& value)
{
    m_progressLabel->setText(value);
}

} // namespace
} // namespace client
} // namespace nx
