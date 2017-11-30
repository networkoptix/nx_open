#pragma once

#include "../event_search_widget.h"
#include "search_widget_base_p.h"

class QnSearchLineEdit;
class QPushButton;

namespace nx {

namespace vms { namespace event { class StringsHelper; }}

namespace client {
namespace desktop {

class UnifiedSearchListModel;

class EventSearchWidget::Private: public detail::EventSearchWidgetPrivateBase
{
    Q_OBJECT
    using base_type = detail::EventSearchWidgetPrivateBase;

public:
    Private(EventSearchWidget* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

private:
    void setupSuperTypeButton();
    void setupEventTypeButton();
    void updateEventTypeButtonVisibility();

private:
    EventSearchWidget* q = nullptr;
    UnifiedSearchListModel* const m_model = nullptr;
    QnSearchLineEdit* const m_searchLineEdit = nullptr;
    QPushButton* const m_superTypeButton = nullptr;
    QPushButton* const m_eventTypeButton = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
};

} // namespace
} // namespace client
} // namespace nx
