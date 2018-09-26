#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLayout>

namespace Ui { class ExportLayoutSettingsWidget; }

class QnMediaResourceWidget;
class QnTimePeriod;

namespace nx {
namespace client {
namespace desktop {

class ExportLayoutSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ExportLayoutSettingsWidget(QWidget* parent = nullptr);
    virtual ~ExportLayoutSettingsWidget();

    struct Data
    {
        bool readOnly = false;
    };

    void setData(const Data& data);

    QLayout* passwordPlaceholder();

signals:
    void dataChanged(Data& data); //< No signal on setData()!

private:
    QScopedPointer<Ui::ExportLayoutSettingsWidget> ui;

    void emitDataChanged();
};

} // namespace desktop
} // namespace client
} // namespace nx
