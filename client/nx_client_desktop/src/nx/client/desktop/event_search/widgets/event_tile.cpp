#include "event_tile.h"
#include "ui_event_tile.h"

#include <QtGui/QPainter>

#include <client/client_globals.h>
#include <ui/common/palette.h>
#include <ui/style/helper.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop
{

namespace
{

static constexpr auto kRoundingRadius = 2;

static constexpr int kTitleFontPixelSize = 13;
static constexpr int kTimestampFontPixelSize = 11;
static constexpr int kDescriptionFontPixelSize = 11;

static constexpr int kTitleFontWeight = QFont::Medium;
static constexpr int kTimestampFontWeight = QFont::Normal;
static constexpr int kDescriptionFontWeight = QFont::Normal;

} // namespace

EventTile::EventTile(QWidget* parent) :
    base_type(parent, Qt::FramelessWindowHint),
    ui(new Ui::EventTile())
{
    ui->setupUi(this);

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

    connect(ui->descriptionLabel, &QLabel::linkActivated, this, &EventTile::linkActivated);
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
        index.data(Qt::DisplayRole).toString(),
        index.data(Qt::DecorationRole).value<QPixmap>(),
        index.data(Qn::TimestampTextRole).toString(),
        index.data(Qn::DescriptionTextRole).toString(),
        parent);

    const auto color = index.data(Qt::ForegroundRole).value<QColor>();
    if (color.isValid())
        tile->setTitleColor(color);

    // TODO: #vkutin Preview?

    // TODO: #vkutin Actions?

    return tile;
}

} // namespace
} // namespace client
} // namespace nx
