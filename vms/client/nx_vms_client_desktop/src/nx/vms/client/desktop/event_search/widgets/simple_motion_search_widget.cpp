// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "simple_motion_search_widget.h"

#include <QtCore/QList>
#include <QtGui/QRegion>
#include <QtWidgets/QAction>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <ui/common/read_only.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/simple_motion_search_list_model.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/utils/log/assert.h>

using nx::vms::client::core::MotionSelection;

namespace nx::vms::client::desktop {

class SimpleMotionSearchWidget::Private: public QObject
{
    Q_DECLARE_TR_FUNCTIONS(SimpleMotionSearchWidget::Private)
    SimpleMotionSearchWidget* const q;

public:
    Private(SimpleMotionSearchWidget* q):
        q(q),
        m_model(qobject_cast<SimpleMotionSearchListModel*>(q->model())),
        m_resourceButton(q->createCustomFilterButton()),
        m_areaButton(q->createCustomFilterButton())
    {
        NX_CRITICAL(m_model);
        q->view()->setHeadersEnabled(false);

        setReadOnly(m_resourceButton, true);
        m_resourceButton->setDeactivatable(false);
        m_resourceButton->setAccented(true);

        setReadOnly(m_areaButton, true); //< Does not affect close button.
        m_areaButton->setAccented(true);
        m_areaButton->setDeactivatedText(tr("Select area on the video to filter results"));
        m_areaButton->setText(tr("In selected area"));
        m_areaButton->setIcon(qnSkin->icon("text_buttons/area.png"));

        connect(m_areaButton, &SelectableTextButton::stateChanged, this,
            [this](SelectableTextButton::State state)
            {
                if (state == SelectableTextButton::State::deactivated)
                    m_model->clearFilterRegions();
            });

        connect(m_model, &SimpleMotionSearchListModel::filterRegionsChanged, this,
            [this]()
            {
                m_areaButton->setState(m_model->isFilterEmpty()
                    ? SelectableTextButton::State::deactivated
                    : SelectableTextButton::State::unselected);
            });

        connect(q->navigator(), &QnWorkbenchNavigator::currentResourceChanged, this,
            &Private::updateResourceButton);

        updateResourceButton();
    }

    SimpleMotionSearchListModel* model() const { return m_model; }

private:
    void updateResourceButton()
    {
        static const auto kTemplate = QString::fromWCharArray(L"%1 \x2013 %2");

        const auto resource = q->navigator()->currentResource();
        const bool isMedia = (bool) resource.dynamicCast<QnMediaResource>();

        const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
        const bool isCamera = camera || !isMedia;

        m_resourceButton->setIcon(isCamera
            ? qnSkin->icon("text_buttons/camera.png")
            : qnSkin->icon("text_buttons/video.png"));

        // TODO: #vkutin Think how to avoid code duplication with AbstractSearchWidget::Private.
        if (resource)
        {
            const auto name = resource->getName();
            if (isCamera)
            {
                const auto baseText = QnDeviceDependentStrings::getNameFromSet(q->resourcePool(),
                    QnCameraDeviceStringSet(tr("Selected device"), tr("Selected camera")),
                    camera);

                m_resourceButton->setText(kTemplate.arg(baseText, name));
            }
            else
            {
                m_resourceButton->setText(kTemplate.arg(tr("Selected media"), name));
            }
        }
        else
        {
            m_resourceButton->setText(kTemplate.arg(tr("Selected camera"),
                tr("none", "No currently selected camera")));
        }
    }

private:
    SimpleMotionSearchListModel* const m_model;
    SelectableTextButton* const m_resourceButton;
    SelectableTextButton* const m_areaButton;
};

SimpleMotionSearchWidget::SimpleMotionSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new SimpleMotionSearchListModel(context), parent),
    d(new Private(this))
{
    setRelevantControls(Control::timeSelector | Control::previewsToggler);
    setPlaceholderPixmap(qnSkin->pixmap("left_panel/placeholders/motion.svg"));

    connect(model(), &AbstractSearchListModel::isOnlineChanged, this,
        &SimpleMotionSearchWidget::updateAllowance);
    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged, this,
        &SimpleMotionSearchWidget::updateAllowance);
}

SimpleMotionSearchWidget::~SimpleMotionSearchWidget()
{
}

SimpleMotionSearchListModel* SimpleMotionSearchWidget::motionModel() const
{
    return d->model();
}

QString SimpleMotionSearchWidget::placeholderText(bool /*constrained*/) const
{
    return makePlaceholderText(tr("No motion detected"),
        tr("Try changing the filters or enable motion recording"));
}

QString SimpleMotionSearchWidget::itemCounterText(int count) const
{
    return tr("%n motion events", "", count);
}

bool SimpleMotionSearchWidget::calculateAllowance() const
{
    // Panel is hidden for live viewers but should be visible when browsing local files offline.
    return !model()->isOnline()
        || accessController()->hasGlobalPermission(GlobalPermission::viewArchive);
}

} // namespace nx::vms::client::desktop
