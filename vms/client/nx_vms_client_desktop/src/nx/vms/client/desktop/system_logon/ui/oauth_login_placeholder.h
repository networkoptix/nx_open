// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <memory>

namespace Ui { class OauthLoginPlaceholder; }

namespace nx::vms::client::desktop {

class OauthLoginPlaceholder: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit OauthLoginPlaceholder(QWidget* parent);
    ~OauthLoginPlaceholder();

signals:
    void retryRequested();

private:
    std::unique_ptr<Ui::OauthLoginPlaceholder> ui;
};

} // namespace nx::vms::client::desktop
