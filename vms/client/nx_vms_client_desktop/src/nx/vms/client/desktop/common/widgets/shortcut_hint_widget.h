#pragma once

#include <QtGui/QKeySequence>
#include <QtWidgets/QLabel>

namespace nx::vms::client::desktop {

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

} // namespace nx::vms::client::desktop
