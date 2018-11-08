#pragma once

#include <QWidget>

namespace Ui { class ExportPasswordWidget; }

namespace nx::vms::client::desktop {


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
    void dataEdited(Data& data); //< No signal on setData()!

private:
    Ui::ExportPasswordWidget* ui;

    void emitDataEdited();
};


} // namespace nx::vms::client::desktop

