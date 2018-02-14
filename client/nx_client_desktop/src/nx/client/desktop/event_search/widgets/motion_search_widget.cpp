#include "motion_search_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>

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
    setMotionSelectionEmpty(true);

    filterEdit()->hide();
    showPreviewsButton()->show();

    connect(m_model, &MotionSearchListModel::totalCountChanged,
        this, &MotionSearchWidget::updateEventCounter);
}

MotionSearchWidget::~MotionSearchWidget()
{
}

void MotionSearchWidget::setMotionSelectionEmpty(bool value)
{
    if (value)
    {
        static const QString kHtmlPlaceholder =
            lit("<center><p>%1</p><p><font size='-3'>%2</font></p></center>")
                .arg(tr("No motion region"))
                .arg(tr("Select some area on camera."));

        setPlaceholderTexts(QString(), kHtmlPlaceholder);
    }
    else
    {
        setPlaceholderTexts(tr("No motion"), tr("No motion detected"));
    }
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

bool MotionSearchWidget::setCurrentTimePeriod(const QnTimePeriod& period)
{
    if (m_model->selectedTimePeriod() == period)
        return false;

    m_model->setSelectedTimePeriod(period);
    return true;
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
