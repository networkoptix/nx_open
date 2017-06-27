#pragma once

#include <ui/graphics/items/resource/resource_widget.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class LayoutPreviewPainter;

namespace workbench {

class LayoutTourItemWidget: public QnResourceWidget
{
    Q_OBJECT
    using base_type = QnResourceWidget;

public:
    LayoutTourItemWidget(
        QnWorkbenchContext* context,
        QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);
    virtual ~LayoutTourItemWidget() override;

protected:
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;
    virtual int calculateButtonsVisibility() const override;

private:
    void initOverlay();

private:
    QSharedPointer<LayoutPreviewPainter> m_previewPainter;
    bool m_updating{false};
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
