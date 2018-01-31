#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/event_search/widgets/unified_search_widget.h>

namespace nx {
namespace client {
namespace desktop {

class MotionSearchListModel;

class MotionSearchWidget: public UnifiedSearchWidget
{
    Q_OBJECT
    using base_type = UnifiedSearchWidget;

public:
    MotionSearchWidget(QWidget* parent = nullptr);
    virtual ~MotionSearchWidget() override;

    void setMotionSearchEnabled(bool value);

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

protected:
    virtual bool hasRelevantTiles() const override;
    void setCurrentTimePeriod(const QnTimePeriod& period) override;

private:
    using base_type::model;
    using base_type::setModel;

private:
    class FilterModel;
    FilterModel* const m_filterModel = nullptr;
    MotionSearchListModel* const m_model = nullptr;
};

} // namespace desktop
} // namespace client
} // namespace nx
