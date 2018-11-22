#include "motion_search_widget.h"

#include <QtCore/QList>
#include <QtGui/QRegion>
#include <QtWidgets/QAction>

#include <core/resource/camera_resource.h>
#include <ui/common/read_only.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/motion_search_list_model.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

class MotionSearchWidget::Private: public QObject
{
    MotionSearchWidget* const q;

public:
    Private(MotionSearchWidget* q):
        q(q),
        m_model(qobject_cast<MotionSearchListModel*>(q->model())),
        m_areaSelectionButton(q->createCustomFilterButton())
    {
        NX_CRITICAL(m_model);

        setReadOnly(m_areaSelectionButton, true); //< Does not affect close button.
        m_areaSelectionButton->setSelectable(false);
        m_areaSelectionButton->setDeactivatable(true);
        m_areaSelectionButton->setAccented(true);
        m_areaSelectionButton->setDeactivatedText(tr("Select area on the video to filter results"));
        m_areaSelectionButton->setText(tr("In selected area"));
        m_areaSelectionButton->setIcon(qnSkin->icon("text_buttons/area.png"));

        connect(m_areaSelectionButton, &SelectableTextButton::stateChanged, this,
            [this](SelectableTextButton::State state)
            {
                if (state == SelectableTextButton::State::deactivated && m_mediaWidget)
                    m_mediaWidget->clearMotionSelection();
            });
    }

    void setMediaWidget(QnMediaResourceWidget* value)
    {
        if (value == m_mediaWidget)
            return;

        m_mediaWidgetConnections = {};

        m_mediaWidget = value;
        updateFilterRegions();

        if (!m_mediaWidget)
            return;

        m_mediaWidgetConnections << connect(m_mediaWidget, &QObject::destroyed, this,
            [this]()
            {
                m_mediaWidget = nullptr;
                updateFilterRegions();
            });

        m_mediaWidgetConnections << connect(
            m_mediaWidget, &QnMediaResourceWidget::motionSelectionChanged,
            this, &Private::updateFilterRegions);
    }

    QList<QRegion> filterRegions() const
    {
        return m_model->filterRegions();
    }

private:
    void updateFilterRegions()
    {
        setFilterRegions(m_mediaWidget ? m_mediaWidget->motionSelection() : QList<QRegion>());

        m_areaSelectionButton->setState(m_mediaWidget && !m_mediaWidget->isMotionSelectionEmpty()
            ? SelectableTextButton::State::unselected
            : SelectableTextButton::State::deactivated);
    }

    void setFilterRegions(const QList<QRegion>& regions)
    {
        if (m_model->filterRegions() == regions)
            return;

        m_model->setFilterRegions(regions);
        emit q->filterRegionsChanged(m_model->filterRegions());
    }

private:
    MotionSearchListModel* const m_model;
    SelectableTextButton* const m_areaSelectionButton;
    QnMediaResourceWidget* m_mediaWidget = nullptr;
    nx::utils::ScopedConnections m_mediaWidgetConnections;
};

MotionSearchWidget::MotionSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new MotionSearchListModel(context), parent),
    d(new Private(this))
{
    setRelevantControls(Control::cameraSelector | Control::timeSelector | Control::previewsToggler);
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/motion.png"));
    selectCameras(AbstractSearchWidget::Cameras::current);
    setCamerasReadOnly(true);
}

MotionSearchWidget::~MotionSearchWidget()
{
}

void MotionSearchWidget::setMediaWidget(QnMediaResourceWidget* value)
{
    d->setMediaWidget(value);
}

QList<QRegion> MotionSearchWidget::filterRegions() const
{
    return d->filterRegions();
}

QString MotionSearchWidget::placeholderText(bool constrained) const
{
    return constrained ? tr("No motion") : tr("No motion detected");
}

QString MotionSearchWidget::itemCounterText(int count) const
{
    return tr("%n motion events", "", count);
}

} // namespace nx::vms::client::desktop
