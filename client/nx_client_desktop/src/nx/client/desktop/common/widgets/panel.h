#pragma once

#include <QtWidgets/QWidget>

namespace nx {
namespace client {
namespace desktop {

class Panel: public QWidget
{
    Q_OBJECT

public:
    explicit Panel(QWidget* parent = nullptr);
};

} // namespace desktop
} // namespace client
} // namespace nx
