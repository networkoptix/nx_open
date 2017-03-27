#include "layout_tours_handler.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QToolButton> // for example. remove me
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <core/resource/layout_resource.h>
#include <nx/client/ui/workbench/layouts/special_layout.h>
#include <nx/client/ui/workbench/layouts/layout_factory.h>
#include <nx/client/ui/workbench/panels/special_layout_panel_widget.h>


namespace {

namespace workbench_alias = nx::client::desktop::ui::workbench;

class LayoutTourResource: public QnLayoutResource
{
};

void registerCreator(workbench_alias::LayoutsFactory* factory)
{
    const auto tourLayoutCreator =
        [](const QnLayoutResourcePtr& resource, QObject* parent) -> QnWorkbenchLayout*
        {
            if (const auto tourResource = resource.dynamicCast<LayoutTourResource>())
                return new workbench_alias::SpecialLayout(tourResource, parent);

            return nullptr;
        };

    factory->addCreator(tourLayoutCreator);
}

} // namespace

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace handlers {

LayoutToursHandler::LayoutToursHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    registerCreator(qnWorkbenchLayoutsFactory);
    connect(action(QnActions::OpenLayoutTourAction), &QAction::triggered,
        this, &LayoutToursHandler::openTousLayout);
}

void LayoutToursHandler::openTousLayout()
{
    const auto tourResource = QnLayoutResourcePtr(new LayoutTourResource());
    const auto layout = qnWorkbenchLayoutsFactory->create(tourResource, this);
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
}

void LayoutToursHandler::setupToursLayout(QnWorkbenchLayout* layout)
{
    const auto special = qobject_cast<workbench_alias::SpecialLayout*>(layout);
    if (!special)
    {
        NX_ASSERT(false, "Invalid layout type");
        return;
    }

    const auto panelWidget = qobject_cast<workbench_alias::SpecialLayoutPanelWidget*>(
        special->panelWidget());
    if (!panelWidget)
    {
        NX_ASSERT(false, "Can't get default special panel");
        return;
    }


    // EXAMPLE
    action(QnActions::OpenLayoutTourAction)->setChecked(false);
    const auto addToolButton =
        [this, panelWidget](const QString& iconPath, const std::function<void()> handler) -> QToolButton*
        {
            const auto button = new QToolButton();
            button->setIcon(qnSkin->icon(iconPath));
            connect(button, &QAbstractButton::clicked, button, handler);
            panelWidget->addButton(button);
            return button;
        };

    const auto toggle = [this]() { action(QnActions::OpenLayoutTourAction)->toggle(); };
    const auto startButton = addToolButton(lit("slider/navigation/play.png"), toggle);
    const auto stopButton = addToolButton(lit("slider/navigation/pause.png"), toggle);

    const auto handleStateChanged =
        [this, panelWidget, startButton, stopButton]()
        {
            static const auto caption = tr("Super Duper Tours Layout Panel (%1)");

            const auto started = action(QnActions::OpenLayoutTourAction)->isChecked();
            startButton->setEnabled(!started);
            stopButton->setEnabled(started);
            panelWidget->setCaption(caption.arg(started ? tr("STARTED") : tr("STOPPED")));
        };

    handleStateChanged();
    connect(action(QnActions::OpenLayoutTourAction), &QAction::toggled, this, handleStateChanged);
}

} // namespace handlers
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx
