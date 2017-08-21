#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class TextOverlaySettingsWidget; }

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class TextOverlaySettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    TextOverlaySettingsWidget(QWidget* parent = nullptr);

    QString text() const;
    void setText(const QString& value);

signals:
    void dataChanged();

private:
    QScopedPointer<Ui::TextOverlaySettingsWidget> ui;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
