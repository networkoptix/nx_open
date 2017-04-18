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

    int order() const;
    void setOrder(int value);

signals:
    void orderChanged(int value);

private:
    void initOverlay();

private:
    QnLayoutResourcePtr m_layout;
    QSharedPointer<LayoutPreviewPainter> m_previewPainter;
    int m_order{0};
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
