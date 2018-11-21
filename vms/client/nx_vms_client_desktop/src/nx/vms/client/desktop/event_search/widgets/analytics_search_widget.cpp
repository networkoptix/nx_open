#include "analytics_search_widget.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource/camera_resource.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>

#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget::Private

class AnalyticsSearchWidget::Private: public QObject
{
    AnalyticsSearchWidget* const q;
    using ButtonState = SelectableTextButton::State;
    using AreaType = QnMediaResourceWidget::AreaType;

public:
    Private(AnalyticsSearchWidget* q):
        q(q),
        m_model(qobject_cast<AnalyticsSearchListModel*>(q->model())),
        m_areaSelectionButton(q->createCustomFilterButton())
    {
        NX_CRITICAL(m_model);

        m_areaSelectionButton->setSelectable(false);
        m_areaSelectionButton->setDeactivatable(true);
        m_areaSelectionButton->setAccented(true);
        m_areaSelectionButton->setDeactivatedText(tr("Select area"));
        m_areaSelectionButton->setIcon(qnSkin->icon("text_buttons/area.png"));

        connect(q, &AbstractSearchWidget::cameraSetChanged, this, &Private::handleStateChange);

        connect(m_areaSelectionButton, &SelectableTextButton::stateChanged,
            this, &Private::handleStateChange);

        connect(m_areaSelectionButton, &SelectableTextButton::clicked, this,
            [this]()
            {
                const bool relevant = m_mediaWidget
                    && m_mediaWidget->areaSelectionType() == AreaType::analytics;

                NX_ASSERT(relevant);
                if (relevant)
                    m_mediaWidget->setAreaSelectionEnabled(true);
            });

        installEventHandler(q, {QEvent::Show, QEvent::Hide}, this, &Private::updateTimelineDisplay);
        connect(q, &AbstractSearchWidget::cameraSetChanged, this, &Private::updateTimelineDisplay);

        connect(q->navigator(), &QnWorkbenchNavigator::currentResourceChanged,
            this, &Private::updateTimelineDisplay);

        connect(q, &AbstractSearchWidget::textFilterChanged, this,
            [this](const QString& text)
            {
                m_model->setFilterText(text);
                updateTimelineDisplay();
            });
    }

    void setCurrentMediaWidget(QnMediaResourceWidget* value)
    {
        if (value && !(value->resource() && value->resource()->hasVideo()))
            value = nullptr;

        if (value == m_mediaWidget)
            return;

        m_mediaWidgetConnections = {};

        if (m_mediaWidget && m_mediaWidget->areaSelectionType() == AreaType::analytics)
            m_mediaWidget->setAreaSelectionType(AreaType::none); //< Must be already disconnected.

        m_mediaWidget = value;
        handleStateChange();

        if (!m_mediaWidget)
            return;

        m_mediaWidgetConnections << connect(m_mediaWidget, &QObject::destroyed, this,
            [this]()
            {
                m_mediaWidget = nullptr;
                handleStateChange();
            });

        m_mediaWidgetConnections << connect(
            m_mediaWidget, &QnMediaResourceWidget::areaSelectionTypeChanged,
            this, &Private::handleStateChange);

        m_mediaWidgetConnections << connect(
            m_mediaWidget, &QnMediaResourceWidget::areaSelectionEnabledChanged,
            this, &Private::handleStateChange);

        m_mediaWidgetConnections << connect(
            m_mediaWidget, &QnMediaResourceWidget::analyticsFilterRectChanged,
            this, &Private::handleStateChange);
    }

    void handleStateChange()
    {
        if (m_updating)
            return;

        QScopedValueRollback updatingGuard(m_updating, true);

        const auto cameras = q->cameras();
        const bool relevant = q->selectedCameras() == Cameras::current && m_mediaWidget;

        m_areaSelectionButton->setVisible(relevant);
        if (relevant)
        {
            m_mediaWidget->setAreaSelectionType(AreaType::analytics);

            const bool selecting = m_mediaWidget->areaSelectionEnabled();
            const auto rect = m_mediaWidget->analyticsFilterRect();
            setFilterRect(rect);

            m_areaSelectionButton->setState(selecting || rect.isValid()
                ? ButtonState::unselected
                : ButtonState::deactivated);

            m_areaSelectionButton->setAccented(selecting);
            m_areaSelectionButton->setText(selecting
                ? tr("Select some area on the video...")
                : tr("In selected area"));
        }
        else
        {
            if (m_mediaWidget)
                m_mediaWidget->setAreaSelectionType(AreaType::none);

            m_areaSelectionButton->deactivate();
            setFilterRect({});
        }
    }

    void updateTimelineDisplay()
    {
        if (!q->isVisible())
        {
            if (q->navigator()->selectedExtraContent() == Qn::AnalyticsContent)
                q->navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
            return;
        }

        const auto currentCamera = q->navigator()->currentResource()
            .dynamicCast<QnVirtualCameraResource>();

        const bool relevant = currentCamera && q->cameras().contains(currentCamera);
        if (!relevant)
        {
            q->navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
            return;
        }

        analytics::storage::Filter filter;
        filter.deviceIds = {currentCamera->getId()};
        filter.boundingBox = m_model->filterRect();
        filter.freeText = q->textFilter();
        q->navigator()->setAnalyticsFilter(filter);
        q->navigator()->setSelectedExtraContent(Qn::AnalyticsContent);
    }

    void setFilterRect(const QRectF& value)
    {
        m_model->setFilterRect(value);
        updateTimelineDisplay();
    }

private:
    AnalyticsSearchListModel* const m_model;
    SelectableTextButton* const m_areaSelectionButton;
    QnMediaResourceWidget* m_mediaWidget = nullptr;
    nx::utils::ScopedConnections m_mediaWidgetConnections;
    bool m_updating = false;
};

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget

AnalyticsSearchWidget::AnalyticsSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new AnalyticsSearchListModel(context), parent),
    d(new Private(this))
{
    setRelevantControls(Control::defaults | Control::footersToggler);
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/analytics.png"));
    selectCameras(AbstractSearchWidget::Cameras::layout);
}

AnalyticsSearchWidget::~AnalyticsSearchWidget()
{
}

void AnalyticsSearchWidget::setCurrentMediaWidget(QnMediaResourceWidget* value)
{
    d->setCurrentMediaWidget(value);
}

QString AnalyticsSearchWidget::placeholderText(bool constrained) const
{
    return constrained ? tr("No objects") : tr("No objects detected");
}

QString AnalyticsSearchWidget::itemCounterText(int count) const
{
    return tr("%n objects", "", count);
}

void AnalyticsSearchWidget::resetFilters()
{
    base_type::resetFilters();
    selectCameras(AbstractSearchWidget::Cameras::layout);
}

} // namespace nx::vms::client::desktop
