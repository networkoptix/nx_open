#include "motion_search_widget.h"

#include <QtCore/QList>
#include <QtGui/QRegion>
#include <QtWidgets/QAction>

#include <core/resource/camera_resource.h>
#include <ui/common/read_only.h>
#include <ui/style/skin.h>

#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/motion_search_list_model.h>
#include <nx/utils/log/assert.h>

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
                if (state == SelectableTextButton::State::deactivated)
                    setFilterRegions({});
            });
    }

    QList<QRegion> filterRegions() const
    {
        return m_model->filterRegions();
    }

    void setFilterRegions(const QList<QRegion>& regions)
    {
        if (m_model->filterRegions() == regions)
            return;

        m_model->setFilterRegions(regions);

        m_areaSelectionButton->setState(m_model->isFilterEmpty()
            ? SelectableTextButton::State::deactivated
            : SelectableTextButton::State::unselected);

        emit q->filterRegionsChanged(m_model->filterRegions());
    }

private:
    MotionSearchListModel* const m_model;
    SelectableTextButton* const m_areaSelectionButton;
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

void MotionSearchWidget::setFilterRegions(const QList<QRegion>& value)
{
    d->setFilterRegions(value);
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
