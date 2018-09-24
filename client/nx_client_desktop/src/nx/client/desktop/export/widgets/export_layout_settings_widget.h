#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

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
        bool cryptVideo = false;
        QString password;
    };

    void setData(const Data& data);

    bool validate();

signals:
    void dataChanged(Data& data);

private:
    QScopedPointer<Ui::ExportLayoutSettingsWidget> ui;

    void emitDataChanged();
};

} // namespace desktop
} // namespace client
} // namespace nx
