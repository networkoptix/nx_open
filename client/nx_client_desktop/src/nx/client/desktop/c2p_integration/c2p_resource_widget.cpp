#include "c2p_resource_widget.h"

#include <QtWebKitWidgets/QWebFrame>
#include <QtWebKitWidgets/QWebPage>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/graphics/items/standard/graphics_web_view.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>

namespace nx {
namespace client {
namespace desktop {

C2pResourceWidget::C2pResourceWidget(
    QnWorkbenchContext* context,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(context, item, parent)
{
    QWebFrame* frame = m_webView->page()->mainFrame();
    connect(frame, &QWebFrame::javaScriptWindowObjectCleared, this,
        [this, frame]()
        {
            frame->addToJavaScriptWindowObject(lit("external"), this);
        });
}

void C2pResourceWidget::c2pplayback(const QString& cameraNames, int timestamp)
{
    qDebug() << "!!!c2pplayback!!!" << cameraNames << timestamp;

    QnVirtualCameraResourceList cameras;
    for (auto name: cameraNames.split(L',', QString::SplitBehavior::SkipEmptyParts))
    {
        name = name.toLower().trimmed();
        if (const auto camera = resourcePool()->getResource<QnVirtualCameraResource>(
            [name](const QnVirtualCameraResourcePtr& resource)
            {
                return resource->getName().toLower() == name;
            }))
        {
            cameras.push_back(camera);
        }
    }
    if (!cameras.empty())
        menu()->trigger(ui::action::DropResourcesAction, cameras);
}

} // namespace desktop
} // namespace client
} // namespace nx
