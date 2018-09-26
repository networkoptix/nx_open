#pragma once

#include <QWidget>

namespace Ui { class ExportPasswordWidget; }

namespace nx {
namespace client {
namespace desktop {


class ExportPasswordWidget: public QWidget
{
    Q_OBJECT

public:
    explicit ExportPasswordWidget(QWidget* parent = 0);
    ~ExportPasswordWidget();

    struct Data
    {
        bool cryptVideo = false;
        QString password;
    };

    void setData(const Data& data);

    bool validate();

signals:
    void dataChanged(Data& data); //< No signal on setData()!

private:
    Ui::ExportPasswordWidget* ui;

    void emitDataChanged();
};


} // namespace desktop
} // namespace client
} // namespace nx

