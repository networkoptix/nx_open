#include "motion_search_widget.h"

#include <QtWidgets/QLabel>

#include <ui/models/sort_filter_list_model.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/search_line_edit.h>

#include <nx/client/desktop/common/models/subset_list_model.h>
#include <nx/client/desktop/event_search/models/motion_search_list_model.h>

namespace nx {
namespace client {
namespace desktop {

MotionSearchWidget::MotionSearchWidget(QWidget* parent):
    base_type(parent),
    m_model(new MotionSearchListModel(this))
{
    setModel(m_model);

    setPlaceholderIcon(qnSkin->pixmap(lit("events/placeholders/motion.png")));
    setMotionSearchEnabled(false);

    filterEdit()->hide();

    connect(m_model, &MotionSearchListModel::totalCountChanged,
        this, &MotionSearchWidget::updateEventCounter);
}

MotionSearchWidget::~MotionSearchWidget()
{
}

void MotionSearchWidget::setMotionSearchEnabled(bool value)
{
    if (value)
        setPlaceholderTexts(tr("No motion"), tr("No motion detected"));
    else
        setPlaceholderTexts(tr("Motion search is turned off"), tr("Motion search is turned off"));
}

bool MotionSearchWidget::isConstrained() const
{
    const auto period = m_model->selectedTimePeriod();
    return period.startTimeMs > 0 || !period.isInfinite();
}

QnVirtualCameraResourcePtr MotionSearchWidget::camera() const
{
    return m_model->camera();
}

void MotionSearchWidget::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    m_model->setCamera(camera);
}

bool MotionSearchWidget::hasRelevantTiles() const
{
    return m_model->rowCount() > 0;
}

void MotionSearchWidget::setCurrentTimePeriod(const QnTimePeriod& period)
{
    m_model->setSelectedTimePeriod(period);
    requestFetch();
}

void MotionSearchWidget::updateEventCounter(int totalCount)
{
    counterLabel()->setText(totalCount
        ? tr("%n motion events", "", totalCount)
        : QString());
}

} // namespace desktop
} // namespace client
} // namespace nx
