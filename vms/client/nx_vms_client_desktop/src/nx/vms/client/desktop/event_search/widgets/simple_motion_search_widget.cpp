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

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kThemeSubstitutions = {
    {QIcon::Normal, {.primary = "light16"}},
    {QIcon::Active, {.primary = "light14"}},
    {QIcon::Selected, {.primary = "light10"}},
    {QnIcon::Pressed, {.primary = "light14"}},
};

NX_DECLARE_COLORIZED_ICON(kFrameIcon, "20x20/Outline/frame.svg", kThemeSubstitutions)

core::SvgIconColorer::ThemeSubstitutions kPlaceholderTheme = {
    {QnIcon::Normal, {.primary = "dark16"}}};

NX_DECLARE_COLORIZED_ICON(
    kMotionPlaceholderIcon, "64x64/Outline/motion.svg", kPlaceholderTheme)

class SimpleMotionSearchWidget::Private: public QObject
{
    Q_DECLARE_TR_FUNCTIONS(SimpleMotionSearchWidget::Private)
    SimpleMotionSearchWidget* const q;

public:
    Private(SimpleMotionSearchWidget* q):
        q(q),
        m_model(qobject_cast<SimpleMotionSearchListModel*>(q->model())),
        m_areaButton(q->createCustomFilterButton())
    {
        NX_CRITICAL(m_model);

        setReadOnly(m_areaButton, true); //< Does not affect close button.
        m_areaButton->setAccented(true);
        m_areaButton->setDeactivatedText(tr("Select area on the video to filter results"));
        m_areaButton->setText(tr("In selected area"));
        m_areaButton->setIcon(qnSkin->icon(kFrameIcon));

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
    }

    SimpleMotionSearchListModel* model() const { return m_model; }

private:
    SimpleMotionSearchListModel* const m_model;
    SelectableTextButton* const m_areaButton;
};

SimpleMotionSearchWidget::SimpleMotionSearchWidget(WindowContext* context, QWidget* parent):
    base_type(context, new SimpleMotionSearchListModel(context), parent),
    d(new Private(this))
{
    setRelevantControls(Control::cameraSelectionDisplay | Control::timeSelector | Control::previewsToggler);
    setPlaceholderPixmap(qnSkin->icon(kMotionPlaceholderIcon).pixmap(64, 64));

    connect(model(), &core::AbstractSearchListModel::isOnlineChanged, this,
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
    return !model()->isOnline() || system()->accessController()->isDeviceAccessRelevant(
        nx::vms::api::AccessRight::viewArchive);
}

} // namespace nx::vms::client::desktop
