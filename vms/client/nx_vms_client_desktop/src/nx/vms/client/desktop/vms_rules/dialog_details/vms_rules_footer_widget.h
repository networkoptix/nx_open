// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class VmsRulesFooterWidget; }

namespace nx::vms::client::desktop {
namespace vms_rules {

class VmsRulesFooterWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    VmsRulesFooterWidget(QWidget* parent = nullptr);
    virtual ~VmsRulesFooterWidget() override;

    QString comment() const;
    void setComment(const QString& comment);

private:
    void setupCommentLabelInteractions();
    void setupIconsAndColors();

private:
    QScopedPointer<Ui::VmsRulesFooterWidget> m_ui;
};

} // namespace vms_rules
} // namespace nx::vms::client::desktop
