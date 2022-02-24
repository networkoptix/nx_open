// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QPushButton>

namespace nx::vms::client::desktop {

class BusyIndicatorWidget;

class BusyIndicatorButton : public QPushButton
{
    Q_OBJECT
    using base_type = QPushButton;

public:
    explicit BusyIndicatorButton(QWidget* parent = nullptr);

    BusyIndicatorWidget* indicator() const;

    void showIndicator(bool show = true);
    void hideIndicator();
    bool isIndicatorVisible() const;

protected:
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

private:
    BusyIndicatorWidget* const m_indicator = nullptr;
};

} // namespace nx::vms::client::desktop
