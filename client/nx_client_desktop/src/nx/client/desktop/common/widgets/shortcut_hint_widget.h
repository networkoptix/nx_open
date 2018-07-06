#pragma once

#include <QtGui/QKeySequence>
#include <QtWidgets/QLabel>

namespace nx {
namespace client {
namespace desktop {

class ShortcutHintWidget: public QLabel
{
    Q_OBJECT
    using base_type = QLabel;

public:
    ShortcutHintWidget(QWidget* parent = nullptr);

    using Description = QPair<QKeySequence, QString>;
    using DescriptionList = QList<Description>;
    void setDescriptions(const DescriptionList& descriptions);
};

} // namespace desktop
} // namespace client
} // namespace nx
