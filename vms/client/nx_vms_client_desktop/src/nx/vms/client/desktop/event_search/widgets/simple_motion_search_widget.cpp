#include "simple_motion_search_widget.h"

#include <QtCore/QList>
#include <QtGui/QRegion>
#include <QtWidgets/QAction>

#include <core/resource/camera_resource.h>
#include <ui/common/read_only.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/simple_motion_search_list_model.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

class SimpleMotionSearchWidget::Private: public QObject
{
    SimpleMotionSearchWidget* const q;

public:
    Private(SimpleMotionSearchWidget* q):
        q(q),
        m_model(qobject_cast<SimpleMotionSearchListModel*>(q->model())),
        m_resourceButton(q->createCustomFilterButton()),
        m_areaButton(q->createCustomFilterButton())
    {
        NX_CRITICAL(m_model);
        q->view()->setHeadersEnabled(false);

        setReadOnly(m_resourceButton, true);
        m_resourceButton->setSelectable(false);
        m_resourceButton->setDeactivatable(false);
        m_resourceButton->setAccented(true);

        setReadOnly(m_areaButton, true); //< Does not affect close button.
        m_areaButton->setSelectable(false);
        m_areaButton->setDeactivatable(true);
        m_areaButton->setAccented(true);
        m_areaButton->setDeactivatedText(tr("Select area on the video to filter results"));
        m_areaButton->setText(tr("In selected area"));
        m_areaButton->setIcon(qnSkin->icon("text_buttons/area.png"));

        connect(m_areaButton, &SelectableTextButton::stateChanged, this,
            [this](SelectableTextButton::State state)
            {
                if (state == SelectableTextButton::State::deactivated)
                    setFilterRegions({});
            });

        connect(q->navigator(), &QnWorkbenchNavigator::currentResourceChanged, this,
            [this]()
            {
                const auto resource = this->q->navigator()->currentResource();

                m_resourceButton->setIcon(resource.dynamicCast<QnSecurityCamResource>()
                    ? qnSkin->icon("text_buttons/camera.png")
                    : qnSkin->icon("text_buttons/video.png"));

                setResourceName(resource.dynamicCast<QnMediaResource>()
                    ? resource->getName()
                    : tr("none"));
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

        m_areaButton->setState(m_model->isFilterEmpty()
            ? SelectableTextButton::State::deactivated
            : SelectableTextButton::State::unselected);

        emit q->filterRegionsChanged(m_model->filterRegions());
    }

private:
    void setResourceName(const QString& value)
    {
        m_resourceButton->setText(QString::fromWCharArray(L"%1 \x2013 %2")
            .arg(tr("Current media"), value));
    }

private:
    SimpleMotionSearchListModel* const m_model;
    SelectableTextButton* const m_resourceButton;
    SelectableTextButton* const m_areaButton;
};

SimpleMotionSearchWidget::SimpleMotionSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new SimpleMotionSearchListModel(context), parent),
    d(new Private(this))
{
    setRelevantControls(Control::timeSelector | Control::previewsToggler);
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/motion.png"));
}

SimpleMotionSearchWidget::~SimpleMotionSearchWidget()
{
}

void SimpleMotionSearchWidget::setFilterRegions(const QList<QRegion>& value)
{
    d->setFilterRegions(value);
}

QList<QRegion> SimpleMotionSearchWidget::filterRegions() const
{
    return d->filterRegions();
}

QString SimpleMotionSearchWidget::placeholderText(bool constrained) const
{
    return constrained ? tr("No motion") : tr("No motion detected");
}

QString SimpleMotionSearchWidget::itemCounterText(int count) const
{
    return tr("%n motion events", "", count);
}

} // namespace nx::vms::client::desktop
