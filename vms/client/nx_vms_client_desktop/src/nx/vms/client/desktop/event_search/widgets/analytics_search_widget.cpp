#include "analytics_search_widget.h"

#include <core/resource/camera_resource.h>
#include <ui/style/skin.h>

#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

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

        connect(q, &AbstractSearchWidget::cameraSetChanged, this,
            [this]() { setAreaSelectionEnabled(this->q->selectedCameras() == Cameras::current); });

        connect(m_areaSelectionButton, &SelectableTextButton::stateChanged,
            [this](SelectableTextButton::State state)
            {
                if (state == SelectableTextButton::State::deactivated)
                    setFilterRect({});
            });

        connect(m_areaSelectionButton, &SelectableTextButton::clicked, this,
            [this]()
            {
                m_areaSelectionButton->setText(tr("Select some area on the video..."));
                m_areaSelectionButton->setAccented(true);
                m_areaSelectionButton->setState(SelectableTextButton::State::unselected);
                emit this->q->areaSelectionRequested({});
            });

        connect(q, &AbstractSearchWidget::textFilterChanged,
            m_model, &AnalyticsSearchListModel::setFilterText);
    }

    QRectF filterRect() const
    {
        return m_model->filterRect();
    }

    void setFilterRect(const QRectF& value)
    {
        if (qFuzzyEquals(m_model->filterRect(), value))
            return;

        m_model->setFilterRect(value);
        updateButtonAppearance();

        emit q->filterRectChanged(m_model->filterRect());
    }

    bool areaSelectionEnabled() const
    {
        return m_areaSelectionEnabled;
    }

private:
    void setAreaSelectionEnabled(bool value)
    {
        if (m_areaSelectionEnabled == value)
            return;

        m_areaSelectionEnabled = value;
        m_areaSelectionButton->setVisible(m_areaSelectionEnabled);
        emit q->areaSelectionEnabledChanged(m_areaSelectionEnabled, {});
    }

    void updateButtonAppearance()
    {
        if (!m_areaSelectionEnabled)
            return;

        m_areaSelectionButton->setState(m_model->filterRect().isValid()
            ? SelectableTextButton::State::unselected
            : SelectableTextButton::State::deactivated);

        m_areaSelectionButton->setAccented(false);
        m_areaSelectionButton->setText(tr("In selected area"));
    }

private:
    AnalyticsSearchListModel* const m_model;
    SelectableTextButton* const m_areaSelectionButton;
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

void AnalyticsSearchWidget::setFilterRect(const QRectF& value)
{
    d->setFilterRect(value.intersected({0, 0, 1, 1}));
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
