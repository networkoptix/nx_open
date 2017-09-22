#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class ExportMediaSettingsWidget; }

class QnMediaResourceWidget;
class QnTimePeriod;

namespace nx {
namespace client {
namespace desktop {

class ExportMediaSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ExportMediaSettingsWidget(QWidget* parent = nullptr);
    virtual ~ExportMediaSettingsWidget() override;

    bool applyFilters() const;
    void setApplyFilters(bool value);

signals:
    void dataChanged(bool applyFilters);

    virtual ~ExportMediaSettingsWidget();

private:
    QScopedPointer<Ui::ExportMediaSettingsWidget> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx
