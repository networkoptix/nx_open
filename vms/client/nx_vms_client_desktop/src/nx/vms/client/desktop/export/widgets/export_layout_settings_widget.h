#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLayout>

namespace Ui { class ExportLayoutSettingsWidget; }

namespace nx::vms::client::desktop {

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
    void dataEdited(Data& data); //< No signal on setData()!

private:
    QScopedPointer<Ui::ExportLayoutSettingsWidget> ui;

    void emitDataEdited();
};

} // namespace nx::vms::client::desktop
