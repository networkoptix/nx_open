#pragma once

#include <QtWidgets/QTableWidget>

namespace nx::vms::client::desktop {

class PaletteWidget: public QTableWidget
{
    Q_OBJECT
    using base_type = QTableWidget;

public:
    explicit PaletteWidget(QWidget* parent = nullptr);

    const QPalette& displayPalette() const;
    void setDisplayPalette(const QPalette& displayPalette);

    void mousePressEvent(QMouseEvent* event) override;

signals:
    void paletteChanged();

private:
    QColor color(int row, int column);
    void setColor(int row, int column, QColor value);

private:
    QPalette m_displayPalette;
};

} // namespace nx::vms::client::desktop
