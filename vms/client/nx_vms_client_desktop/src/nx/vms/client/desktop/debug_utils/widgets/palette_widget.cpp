#include "palette_widget.h"

#include <QtGui/QClipboard>
#include <QtGui/QMouseEvent>

#include <QtWidgets/QApplication>
#include <QtWidgets/QColorDialog>

#include <nx/utils/random.h>

namespace {

struct ColorRole
{
    const char* name;
    QPalette::ColorRole role;
};

const ColorRole colorRoles[] = {
    {"Window", QPalette::Window},
    {"WindowText", QPalette::WindowText},
    {"Base", QPalette::Base},
    {"AlternateBase", QPalette::AlternateBase},
    {"Text", QPalette::Text},
    {"Button", QPalette::Button},
    {"ButtonText", QPalette::ButtonText},
    {"BrightText", QPalette::BrightText},
    {"Light", QPalette::Light},
    {"Midlight", QPalette::Midlight},
    {"Dark", QPalette::Dark},
    {"Mid", QPalette::Mid},
    {"Shadow", QPalette::Shadow},
    {"Highlight", QPalette::Highlight},
    {"HighlightedText", QPalette::HighlightedText},
    {"Link", QPalette::Link},
    {"LinkVisited", QPalette::LinkVisited},
    {"ToolTipBase", QPalette::ToolTipBase},
    {"ToolTipText", QPalette::ToolTipText}
};

struct ColorGroup
{
    const char* name;
    QPalette::ColorGroup group;
};

const ColorGroup colorGroups[] = {
    {"Active", QPalette::Active},
    {"Inactive", QPalette::Inactive},
    {"Disabled", QPalette::Disabled},
};

const int roleCount = sizeof(colorRoles) / sizeof(colorRoles[0]);
const int groupCount = sizeof(colorGroups) / sizeof(colorGroups[0]);

} // namespace

namespace nx::vms::client::desktop {

PaletteWidget::PaletteWidget(QWidget* parent):
    base_type(parent),
    m_displayPalette(Qt::transparent)
{
    setSelectionMode(QTableWidget::NoSelection);
    setSortingEnabled(false);

    setRowCount(roleCount);
    setColumnCount(groupCount);

    for (int c = 0; c < groupCount; c++)
        setHorizontalHeaderItem(c, new QTableWidgetItem(QLatin1String(colorGroups[c].name)));

    for (int r = 0; r < roleCount; r++)
        setVerticalHeaderItem(r, new QTableWidgetItem(QLatin1String(colorRoles[r].name)));

    setDisplayPalette(palette());

    connect(this, &QTableWidget::cellClicked, this,
        [this](int row, int column)
        {
            QColor color = this->color(row, column);
            qApp->clipboard()->setText(color.name());
        });

    connect(this, &QTableWidget::cellDoubleClicked, this,
        [this](int row, int column)
        {
            const auto updated = QColorDialog::getColor(color(row, column), this);
            if (updated.isValid())
                setColor(row, column, updated);
        });
}

const QPalette& PaletteWidget::displayPalette() const
{
    return m_displayPalette;
}

void PaletteWidget::setDisplayPalette(const QPalette& displayPalette)
{
    if (m_displayPalette == displayPalette)
        return;

    m_displayPalette = displayPalette;

    for (int r = 0; r < roleCount; ++r)
    {
        for (int c = 0; c < groupCount; ++c)
        {
            QColor color = this->color(r, c);

            QPixmap pixmap(32, 32);
            pixmap.fill(color);

            QString rgb;
            rgb.sprintf("#%02x%02x%02x", color.red(), color.green(), color.blue());

            setItem(r, c, new QTableWidgetItem(QIcon(pixmap), rgb));
        }
    }

    emit paletteChanged();
}

void PaletteWidget::mousePressEvent(QMouseEvent* event)
{
    base_type::mousePressEvent(event);
    if (event->button() != Qt::MouseButton::RightButton)
        return;

    QPoint pos = event->pos();
    QPersistentModelIndex index = indexAt(pos);

    QColor c;
    c.setRed(nx::utils::random::number(0, 256));
    c.setGreen(nx::utils::random::number(0, 256));
    c.setBlue(nx::utils::random::number(0, 256));
    setColor(index.row(), index.column(), c);
}

QColor PaletteWidget::color(int row, int column)
{
    return m_displayPalette.color(colorGroups[column].group, colorRoles[row].role);
}

void PaletteWidget::setColor(int row, int column, QColor value)
{
    // TODO: #GDM Set delegate instead of this magic.
    QPalette p = m_displayPalette;
    p.setColor(colorGroups[column].group, colorRoles[row].role, value);
    setDisplayPalette(p);
    update();
}

} // namespace nx::vms::client::desktop
