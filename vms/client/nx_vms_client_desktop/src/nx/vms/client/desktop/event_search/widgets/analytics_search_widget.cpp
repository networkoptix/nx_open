#include "analytics_search_widget.h"

#include <core/resource/camera_resource.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>

#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget::Private

class AnalyticsSearchWidget::Private: public QObject
{
    AnalyticsSearchWidget* const q;

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
        m_areaSelectionButton->hide();

        connect(q, &AbstractSearchWidget::cameraSetChanged, this, &Private::updateAreaRelevancy);

        connect(m_areaSelectionButton, &SelectableTextButton::stateChanged,
            [this](SelectableTextButton::State state)
            {
                if (state == SelectableTextButton::State::deactivated && m_mediaWidget)
                    m_mediaWidget->setAnalyticsFilterRect({});
            });

        connect(m_areaSelectionButton, &SelectableTextButton::clicked, this,
            [this]() { emit this->q->areaSelectionRequested({}); });

        connect(q, &AbstractSearchWidget::textFilterChanged,
            m_model, &AnalyticsSearchListModel::setFilterText);
    }

    void setMediaWidget(QnMediaResourceWidget* value)
    {
        if (value && !(value->resource() && value->resource()->hasVideo()))
            value = nullptr;

        if (value == m_mediaWidget)
            return;

        m_mediaWidgetConnections = {};

        m_mediaWidget = value;
        updateAreaRelevancy();

        if (!m_mediaWidget)
            return;

        m_mediaWidgetConnections << connect(m_mediaWidget, &QObject::destroyed, this,
            [this]()
            {
                m_mediaWidget = nullptr;
                updateAreaRelevancy();
            });

        m_mediaWidgetConnections << connect(
            m_mediaWidget, &QnMediaResourceWidget::areaSelectionEnabledChanged,
            this, &Private::updateButtonAppearance);

        m_mediaWidgetConnections << connect(
            m_mediaWidget, &QnMediaResourceWidget::analyticsFilterRectChanged,
            this, &Private::updateAreaRelevancy);
    }

    QRectF filterRect() const
    {
        return m_model->filterRect();
    }

    bool areaSelectionEnabled() const
    {
        return m_areaSelectionEnabled;
    }

private:
    void setFilterRect(const QRectF& value)
    {
        if (qFuzzyEquals(m_model->filterRect(), value))
            return;

        m_model->setFilterRect(value);
        updateButtonAppearance();

        emit q->filterRectChanged(m_model->filterRect());
    }

    void updateAreaRelevancy()
    {
        const bool relevant = q->selectedCameras() == Cameras::current && m_mediaWidget;
        if (m_areaSelectionEnabled != relevant)
        {
            m_areaSelectionEnabled = relevant;
            m_areaSelectionButton->setVisible(m_areaSelectionEnabled);
            emit q->areaSelectionEnabledChanged(m_areaSelectionEnabled, {});
        }

        setFilterRect(m_areaSelectionEnabled ? m_mediaWidget->analyticsFilterRect() : QRectF());
    }

    void updateButtonAppearance()
    {
        if (!m_areaSelectionEnabled)
            return;

        const bool selecting = m_mediaWidget && m_mediaWidget->areaSelectionEnabled();
        m_areaSelectionButton->setState(selecting || m_model->filterRect().isValid()
            ? SelectableTextButton::State::unselected
            : SelectableTextButton::State::deactivated);

        m_areaSelectionButton->setAccented(selecting);
        m_areaSelectionButton->setText(selecting
            ? tr("Select some area on the video...")
            : tr("In selected area"));
    }

private:
    AnalyticsSearchListModel* const m_model;
    SelectableTextButton* const m_areaSelectionButton;
    QnMediaResourceWidget* m_mediaWidget = nullptr;
    nx::utils::ScopedConnections m_mediaWidgetConnections;
    bool m_areaSelectionEnabled = false;
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

void AnalyticsSearchWidget::setMediaWidget(QnMediaResourceWidget* value)
{
    d->setMediaWidget(value);
}

QRectF AnalyticsSearchWidget::filterRect() const
{
    return d->filterRect();
}

bool AnalyticsSearchWidget::areaSelectionEnabled() const
{
    return d->areaSelectionEnabled();
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
