#include "layout_tours_handler.h"

#include <QtWidgets/QAction>

#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <core/resource/layout_resource.h>
#include <nx/client/ui/workbench/layouts/special_layout.h>
#include <nx/client/ui/workbench/layouts/layout_factory.h>


namespace {

namespace w = nx::client::desktop::ui::workbench;

class LayoutTourResource: public QnLayoutResource
{
};

void registerCreator()
{
    const auto tourLayoutCreator =
        [](const QnLayoutResourcePtr& resource, QObject* parent) -> QnWorkbenchLayout*
        {
            if (const auto tourResource = resource.dynamicCast<LayoutTourResource>())
                return new w::SpecialLayout(tourResource, parent);

            return nullptr;
        };

    qnLayoutFactory->addCreator(tourLayoutCreator);
}

} // namespace

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace handlers {

LayoutToursHandler::LayoutToursHandler(QObject* parent):
    base_type(parent),
    QObject(parent)
{
    registerCreator();

    const auto setupToursLayout =
        [](QnWorkbenchLayout* layout)
        {
            const auto special = qobject_cast<w::SpecialLayout*>(layout);
            if (!special)
            {
                NX_ASSERT(false, "Invalid layout type");
                return;
            }
            special->setPanelCaption(tr("Super Tours Layout"));
        };

    connect(action(QnActions::OpenLayoutTourAction), &QAction::triggered, this,
        [this, setupToursLayout]()
        {
            const auto tourResource = QnLayoutResourcePtr(new LayoutTourResource());
            const auto layout = qnLayoutFactory->create(tourResource, this);
            if (!layout)
            {
                NX_ASSERT(false, "Can't create layout tours");
                return;
            }

            layout->setName(lit("Test Layout Tours"));
            layout->setIcon(qnSkin->icon("tree/videowall.png"));
            setupToursLayout(layout);

            workbench()->addLayout(layout);
            workbench()->setCurrentLayout(layout);
        });
}

} // namespace handlers
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx
