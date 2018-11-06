#pragma once

#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

class Panel: public QWidget
{
    Q_OBJECT

public:
    explicit Panel(QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop
