#include "layout_tours_handler.h"

#include <QtWidgets/QAction>

#include <ui/workbench/workbench.h>
#include <core/resource/layout_resource.h>
#include <nx/client/ui/workbench/layouts/special_layout.h>
#include <nx/client/ui/workbench/layouts/layout_factory.h>

namespace {

class LayoutTourResource: public QnLayoutResource
{

};

void registerCreator()
{
    const auto tourLayoutCreator =
        [](const QnLayoutResourcePtr& resource, QObject* parent) -> QnWorkbenchLayout*
        {
            if (const auto tourResource = resource.dynamicCast<LayoutTourResource>())
                return new nx::client::ui::workbench::layouts::SpecialLayout(tourResource, parent);

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

    connect(action(QnActions::OpenLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            const auto layout =
                qnLayoutFactory->create(QnLayoutResourcePtr(new LayoutTourResource()), this);
            layout->setName(lit("Test Layout Tours"));
            if (layout)
                workbench()->addLayout(layout);
            else
                NX_ASSERT(false, "Can't create layout tours");
        });
}

} // namespace handlers
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx
