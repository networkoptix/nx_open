// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include "../model_view/rules_table_model.h"

namespace Ui { class FooterWidget; }

namespace nx::vms::client::desktop::rules {

class FooterWidget: public QWidget
{
    Q_OBJECT

public:
    explicit FooterWidget(QWidget* parent = nullptr);
    virtual ~FooterWidget() override;

    void setRule(std::weak_ptr<SimplifiedRule> rule);

private:
    void setupCommentLabelInteractions();
    void setupIconsAndColors();
    void updateUi();
    void displayComment(const QString& comment);

private:
    QScopedPointer<Ui::FooterWidget> ui;

    std::weak_ptr<SimplifiedRule> displayedRule;
};

} // namespace nx::vms::client::desktop::rules
