// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QPushButton>

namespace nx::vms::client::desktop {

class BusyIndicatorWidget;

class BusyIndicatorButton: public QPushButton
{
    Q_OBJECT
    using base_type = QPushButton;

public:
    explicit BusyIndicatorButton(QWidget* parent = nullptr);

    BusyIndicatorWidget* indicator() const;

    bool isBusy() const;
    void setBusy(bool busy);

    bool isIndicatorVisible() const;
    void setIndicatorVisible(bool visible);

protected:
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

private:
    BusyIndicatorWidget* const m_indicator = nullptr;
};

} // namespace nx::vms::client::desktop
