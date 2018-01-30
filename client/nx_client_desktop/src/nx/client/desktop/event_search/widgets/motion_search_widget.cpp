#include "motion_search_widget.h"

#include <ui/models/sort_filter_list_model.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/search_line_edit.h>

#include <nx/client/desktop/common/models/subset_list_model.h>
#include <nx/client/desktop/event_search/models/motion_search_list_model.h>

namespace nx {
namespace client {
namespace desktop {

class MotionSearchWidget::FilterModel: public QnSortFilterListModel
{
    using base_type = QnSortFilterListModel;

public:
    using base_type::base_type;

    QnTimePeriod selectedTimePeriod() const
    {
        return m_selectedTimePeriod;
    }

    void setSelectedTimePeriod(const QnTimePeriod& value)
    {
        if (m_selectedTimePeriod == value)
            return;

        m_selectedTimePeriod = value;
        forceUpdate();
    }

protected:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        if (!m_selectedTimePeriod.isValid() || sourceParent.isValid() || !sourceModel())
            return false;

        const auto index = sourceModel()->index(sourceRow, 0, QModelIndex());

        const auto timestampMs = sourceModel()->data(index, Qn::TimestampRole).value<qint64>();
        const auto durationMs = sourceModel()->data(index, Qn::DurationRole).value<qint64>();

        const QnTimePeriod period(timestampMs, durationMs);
        return timestampMs > 0 && m_selectedTimePeriod.intersects(period);
    }

private:
    QnTimePeriod m_selectedTimePeriod =
        QnTimePeriod(QnTimePeriod::kMinTimeValue, QnTimePeriod::infiniteDuration());
};

MotionSearchWidget::MotionSearchWidget(QWidget* parent):
    base_type(parent),
    m_filterModel(new FilterModel(this)),
    m_model(new MotionSearchListModel(this))
{
    m_filterModel->setSourceModel(m_model);
    setModel(new SubsetListModel(m_filterModel, 0, QModelIndex(), this));

    setPlaceholderIcon(qnSkin->pixmap(lit("events/placeholders/motion.png")));
    setMotionSearchEnabled(false);

    filterEdit()->hide();
}

MotionSearchWidget::~MotionSearchWidget()
{
}

void MotionSearchWidget::setMotionSearchEnabled(bool value)
{
    setPlaceholderText(value
        ? tr("No motion")
        : tr("Motion search is turned off"));
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
    return m_filterModel && m_filterModel->rowCount() > 0;
}

void MotionSearchWidget::setCurrentTimePeriod(const QnTimePeriod& period)
{
    m_filterModel->setSelectedTimePeriod(period);
    requestFetch();
}

} // namespace desktop
} // namespace client
} // namespace nx
