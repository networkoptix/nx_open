// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/graphics/items/resource/resource_widget.h>

namespace nx::vms::client::desktop {

namespace ui { class LayoutPreviewPainter; }

class ShowreelItemWidget: public QnResourceWidget
{
    Q_OBJECT
    using base_type = QnResourceWidget;

public:
    ShowreelItemWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        nx::vms::client::desktop::WindowContext* windowContext,
        QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);
    virtual ~ShowreelItemWidget() override;

protected:
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;
    virtual int calculateButtonsVisibility() const override;

private:
    void initOverlay();

private:
    QSharedPointer<ui::LayoutPreviewPainter> m_previewPainter;
    bool m_updating{false};
};

} // namespace nx::vms::client::desktop
