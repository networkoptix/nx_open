#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLayout>

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
    virtual ~ExportMediaSettingsWidget();

    struct Data
    {
        bool applyFilters = true;
        bool transcodingAllowed = true;
    };

    void setData(const Data& data);

    QLayout* passwordPlaceholder();

signals:
    void dataEdited(Data& data); //< No signal on setData()!

private:
    QScopedPointer<Ui::ExportMediaSettingsWidget> ui;

    void emitDataEdited();
};

} // namespace desktop
} // namespace client
} // namespace nx
