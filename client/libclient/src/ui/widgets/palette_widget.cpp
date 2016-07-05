#include "palette_widget.h"

namespace {

    struct ColorRole {
        const char *name;
        QPalette::ColorRole role;
    };

    const ColorRole colorRoles[] = {
        { "Window",          QPalette::Window },
        { "WindowText",      QPalette::WindowText },
        { "Base",            QPalette::Base },
        { "AlternateBase",   QPalette::AlternateBase },
        { "Text",            QPalette::Text },
        { "Button",          QPalette::Button },
        { "ButtonText",      QPalette::ButtonText },
        { "BrightText",      QPalette::BrightText },
        { "Light",           QPalette::Light },
        { "Midlight",        QPalette::Midlight },
        { "Dark",            QPalette::Dark },
        { "Mid",             QPalette::Mid },
        { "Shadow",          QPalette::Shadow },
        { "Highlight",       QPalette::Highlight },
        { "HighlightedText", QPalette::HighlightedText },
        { "Link",            QPalette::Link },
        { "LinkVisited",     QPalette::LinkVisited },
        { "ToolTipBase",     QPalette::ToolTipBase },
        { "ToolTipText",     QPalette::ToolTipText }
    };

    struct ColorGroup {
        const char *name;
        QPalette::ColorGroup group;
    };

    const ColorGroup colorGroups[] = {
        { "Active",    QPalette::Active },
        { "Inactive",  QPalette::Inactive },
        { "Disabled",  QPalette::Disabled },
    };

    const int roleCount = sizeof(colorRoles) / sizeof(colorRoles[0]);
    const int groupCount = sizeof(colorGroups) / sizeof(colorGroups[0]);

} // anonymous namespace

QnPaletteWidget::QnPaletteWidget(QWidget *parent):
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
}

QnPaletteWidget::~QnPaletteWidget() {
    return;
}

const QPalette &QnPaletteWidget::displayPalette() const {
    return m_displayPalette;
}

void QnPaletteWidget::setDisplayPalette(const QPalette &displayPalette) {
    if(m_displayPalette == displayPalette)
        return;

    m_displayPalette = displayPalette;

    for (int r = 0; r < roleCount; ++r) {
        for (int c = 0; c < groupCount; ++c) {
            QColor color = displayPalette.color(colorGroups[c].group, colorRoles[r].role);
            
            QPixmap pixmap(32, 32);
            pixmap.fill(color);

            QString rgb;
            rgb.sprintf("#%02x%02x%02x", color.red(), color.green(), color.blue());
            
            setItem(r, c, new QTableWidgetItem(QIcon(pixmap), rgb));
        }
    }
}
