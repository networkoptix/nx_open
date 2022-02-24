// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtWidgets/QWidget>
#include <nx/utils/impl_ptr.h>

namespace Ui { class RewindForWidget; }

namespace nx::vms::client::desktop {

class RewindForWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit RewindForWidget(QWidget* parent = nullptr);
    virtual ~RewindForWidget() override;

    /**
     * @return 0 if checkbox is not set, actual value otherwise.
     */
    std::chrono::seconds value() const;

    /**
     * Clears checkbox if 0 is passed, otherwise checks it and resets spinbox to the given value.
     */
    void setValue(std::chrono::seconds value);

signals:
    /* Emitted when value is explicitly changed by user. */
    void valueEdited();

private:
    nx::utils::ImplPtr<Ui::RewindForWidget> ui;
};

} // namespace nx::vms::client::desktop
