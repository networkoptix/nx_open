// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "simple_motion_search_widget.h"

#include <QtCore/QList>
#include <QtGui/QAction>
#include <QtGui/QRegion>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/simple_motion_search_list_model.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/common/read_only.h>
#include <ui/workbench/workbench_navigator.h>

using nx::vms::client::core::MotionSelection;
using nx::vms::client::core::AccessController;

namespace nx::vms::client::desktop {

static const QColor klight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{klight16Color, "light16"}}},
    {QIcon::Active, {{klight16Color, "light14"}}},
    {QIcon::Selected, {{klight16Color, "light10"}}},
    {QnIcon::Pressed, {{klight16Color, "light14"}}},
};

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
        m_areaButton->setIcon(qnSkin->icon("text_buttons/frame_20x20_deprecated.svg", kIconSubstitutions));

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
        const auto resource = q->navigator()->currentResource();
        const bool isMedia = (bool) resource.dynamicCast<QnMediaResource>();

        const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
        const bool isCamera = camera || !isMedia;

        m_resourceButton->setIcon(isCamera
            ? qnSkin->icon("text_buttons/camera_20.svg", kIconSubstitutions)
            : qnSkin->icon("text_buttons/play_20.svg", kIconSubstitutions));

        // TODO: #vkutin Think how to avoid code duplication with AbstractSearchWidget::Private.
        if (resource)
        {
            const auto name = resource->getName();
            if (isCamera)
            {
                const auto baseText = QnDeviceDependentStrings::getNameFromSet(q->resourcePool(),
                    QnCameraDeviceStringSet(tr("Selected device"), tr("Selected camera")),
                    camera);

                m_resourceButton->setText(
                    QString("%1 %2 %3").arg(baseText, nx::UnicodeChars::kEnDash, name));
            }
            else
            {
                m_resourceButton->setText(
                    QString("%1 %2 %3").arg(tr("Selected media"), nx::UnicodeChars::kEnDash, name));
            }
        }
        else
        {
            m_resourceButton->setText(QString("%1 %2 %3").arg(
                tr("Selected camera"),
                nx::UnicodeChars::kEnDash,
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
    return !model()->isOnline() || systemContext()->accessController()->isDeviceAccessRelevant(
        nx::vms::api::AccessRight::viewArchive);
}

} // namespace nx::vms::client::desktop
