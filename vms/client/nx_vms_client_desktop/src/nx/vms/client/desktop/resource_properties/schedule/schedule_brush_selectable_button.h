// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QVariant>

#include <nx/vms/client/desktop/common/widgets/selectable_button.h>

namespace nx::vms::client::desktop {

class ScheduleCellPainter;

class ScheduleBrushSelectableButton: public SelectableButton
{
    Q_OBJECT
    using base_type = SelectableButton;

public:
    ScheduleBrushSelectableButton(QWidget* parent = nullptr);
    virtual ~ScheduleBrushSelectableButton() override;

    QVariant buttonBrush() const;
    void setButtonBrush(const QVariant& brushData);

    ScheduleCellPainter* cellPainter() const;
    void setCellPainter(ScheduleCellPainter* cellPainter);

private:
    QVariant m_buttonBrush;
    ScheduleCellPainter* m_cellPainter;
};

} // namespace nx::vms::client::desktop
