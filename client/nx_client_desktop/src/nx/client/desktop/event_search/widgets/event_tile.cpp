#include "event_tile.h"
#include "ui_event_tile.h"

#include <QtGui/QPainter>

#include <client/client_globals.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/common/palette.h>
#include <ui/common/widget_anchor.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>

#include <nx/client/desktop/ui/actions/action.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <nx/utils/log/assert.h>

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

} // namespace

EventTile::EventTile(const QnUuid& id, QWidget* parent):
    base_type(parent, Qt::FramelessWindowHint),
    ui(new Ui::EventTile()),
    m_closeButton(new QPushButton(this)),
    m_id(id)
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
    ui->previewLabel->setHidden(true);

    ui->nameLabel->setForegroundRole(QPalette::Text);
    ui->timestampLabel->setForegroundRole(QPalette::WindowText);
    ui->descriptionLabel->setForegroundRole(QPalette::Light);

    QFont font;
    font.setWeight(kTitleFontWeight);
    font.setPixelSize(kTitleFontPixelSize);
    ui->nameLabel->setFont(font);
    ui->nameLabel->setProperty(style::Properties::kDontPolishFontProperty, true);

    font.setWeight(kTimestampFontWeight);
    font.setPixelSize(kTimestampFontPixelSize);
    ui->timestampLabel->setFont(font);
    ui->timestampLabel->setProperty(style::Properties::kDontPolishFontProperty, true);

    font.setWeight(kDescriptionFontWeight);
    font.setPixelSize(kDescriptionFontPixelSize);
    ui->descriptionLabel->setFont(font);
    ui->descriptionLabel->setProperty(style::Properties::kDontPolishFontProperty, true);

    connect(m_closeButton, &QPushButton::clicked, this, &EventTile::closeRequested);

    connect(ui->descriptionLabel, &QLabel::linkActivated, this, &EventTile::linkActivated);
}

EventTile::EventTile(
    const QnUuid& id,
    const QString& title,
    const QPixmap& icon,
    const QString& timestamp,
    const QString& description,
    QWidget* parent)
    :
    EventTile(id, parent)
{
    setTitle(title);
    setIcon(icon);
    setTimestamp(timestamp);
    setDescription(description);
}

EventTile::~EventTile()
{
}

QnUuid EventTile::id() const
{
    return m_id;
}

QString EventTile::title() const
{
    return ui->nameLabel->text();
}

void EventTile::setTitle(const QString& value)
{
    ui->nameLabel->setText(value);
    ui->nameLabel->setHidden(value.isEmpty());
}

QColor EventTile::titleColor() const
{
    ui->nameLabel->ensurePolished();
    return ui->nameLabel->palette().color(QPalette::Text);
}

void EventTile::setTitleColor(const QColor& value)
{
    ui->nameLabel->ensurePolished();
    setPaletteColor(ui->nameLabel, QPalette::Text, value);
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
    ui->timestampLabel->setHidden(value.isEmpty());
}

QPixmap EventTile::icon() const
{
    if (const auto pixmapPtr = ui->iconLabel->pixmap())
        return *pixmapPtr;

    return QPixmap();
}

void EventTile::setIcon(const QPixmap& value)
{
    ui->iconLabel->setPixmap(value);
    // Icon label is always visible. It keeps column width fixed.
}

QPixmap EventTile::image() const
{
    if (const auto pixmapPtr = ui->previewLabel->pixmap())
        return *pixmapPtr;

    return QPixmap();
}

void EventTile::setImage(const QPixmap& value)
{
    ui->previewLabel->setPixmap(value);
    ui->previewLabel->setHidden(value.isNull());
}

QAction* EventTile::action() const
{
    return ui->actionButton->action();
}

void EventTile::setAction(QAction* value)
{
    ui->actionButton->setAction(value);
}

void EventTile::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().window());
    painter.drawRoundedRect(rect(), kRoundingRadius, kRoundingRadius);
}

EventTile* EventTile::createFrom(const QModelIndex& index, QWidget* parent)
{
    NX_ASSERT(index.isValid());

    auto tile = new EventTile(
        index.data(Qn::UuidRole).value<QnUuid>(),
        index.data(Qt::DisplayRole).toString(),
        index.data(Qt::DecorationRole).value<QPixmap>(),
        index.data(Qn::TimestampTextRole).toString(),
        index.data(Qn::DescriptionTextRole).toString(),
        parent);

    tile->setToolTip(index.data(Qt::ToolTipRole).toString());

    setHelpTopic(tile, index.data(Qn::HelpTopicIdRole).toInt());

    const auto color = index.data(Qt::ForegroundRole).value<QColor>();
    if (color.isValid())
        tile->setTitleColor(color);

    // TODO: #vkutin Preview?

    return tile;
}

bool EventTile::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::Enter:
        case QEvent::HoverEnter:
            ui->timestampLabel->setHidden(true);
            m_closeButton->setHidden(false);
            break;

        case QEvent::Leave:
        case QEvent::HoverLeave:
            ui->timestampLabel->setHidden(false);
            m_closeButton->setHidden(true);
            break;

        case QEvent::MouseButtonPress:base_type::event(event);
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

} // namespace
} // namespace client
} // namespace nx
