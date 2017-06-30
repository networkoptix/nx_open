#include "animations_control_dialog.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QBoxLayout>

#include <nx/client/desktop/ui/workbench/workbench_animations.h>

#include <ui/workaround/widgets_signals_workaround.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

using namespace workbench;

namespace {

QComboBox* createEasingComboBox(QWidget* parent)
{
    QComboBox* result = new QComboBox(parent);
    result->addItem(lit("Linear"), QEasingCurve::Linear);
    result->addItem(lit("InQuad"), QEasingCurve::InQuad);
    result->addItem(lit("OutQuad"), QEasingCurve::OutQuad);
    result->addItem(lit("InOutQuad"), QEasingCurve::InOutQuad);
    result->addItem(lit("OutInQuad"), QEasingCurve::OutInQuad);
    result->addItem(lit("InCubic"), QEasingCurve::InCubic);
    result->addItem(lit("OutCubic"), QEasingCurve::OutCubic);
    result->addItem(lit("InOutCubic"), QEasingCurve::InOutCubic);
    result->addItem(lit("OutInCubic"), QEasingCurve::OutInCubic);
    result->addItem(lit("InQuart"), QEasingCurve::InQuart);
    result->addItem(lit("OutQuart"), QEasingCurve::OutQuart);
    result->addItem(lit("InOutQuart"), QEasingCurve::InOutQuart);
    result->addItem(lit("OutInQuart"), QEasingCurve::OutInQuart);
    result->addItem(lit("InQuint"), QEasingCurve::InQuint);
    result->addItem(lit("OutQuint"), QEasingCurve::OutQuint);
    result->addItem(lit("InOutQuint"), QEasingCurve::InOutQuint);
    result->addItem(lit("OutInQuint"), QEasingCurve::OutInQuint);
    result->addItem(lit("InSine"), QEasingCurve::InSine);
    result->addItem(lit("OutSine"), QEasingCurve::OutSine);
    result->addItem(lit("InOutSine"), QEasingCurve::InOutSine);
    result->addItem(lit("OutInSine"), QEasingCurve::OutInSine);
    result->addItem(lit("InExpo"), QEasingCurve::InExpo);
    result->addItem(lit("OutExpo"), QEasingCurve::OutExpo);
    result->addItem(lit("InOutExpo"), QEasingCurve::InOutExpo);
    result->addItem(lit("OutInExpo"), QEasingCurve::OutInExpo);
    result->addItem(lit("InCirc"), QEasingCurve::InCirc);
    result->addItem(lit("OutCirc"), QEasingCurve::OutCirc);
    result->addItem(lit("InOutCirc"), QEasingCurve::InOutCirc);
    result->addItem(lit("OutInCirc"), QEasingCurve::OutInCirc);
    result->addItem(lit("InElastic"), QEasingCurve::InElastic);
    result->addItem(lit("OutElastic"), QEasingCurve::OutElastic);
    result->addItem(lit("InOutElastic"), QEasingCurve::InOutElastic);
    result->addItem(lit("OutInElastic"), QEasingCurve::OutInElastic);
    result->addItem(lit("InBack"), QEasingCurve::InBack);
    result->addItem(lit("OutBack"), QEasingCurve::OutBack);
    result->addItem(lit("InOutBack"), QEasingCurve::InOutBack);
    result->addItem(lit("OutInBack"), QEasingCurve::OutInBack);
    result->addItem(lit("InBounce"), QEasingCurve::InBounce);
    result->addItem(lit("OutBounce"), QEasingCurve::OutBounce);
    result->addItem(lit("InOutBounce"), QEasingCurve::InOutBounce);
    result->addItem(lit("OutInBounce"), QEasingCurve::OutInBounce);
    result->addItem(lit("InCurve"), QEasingCurve::InCurve);
    result->addItem(lit("OutCurve"), QEasingCurve::OutCurve);
    result->addItem(lit("SineCurve"), QEasingCurve::SineCurve);
    result->addItem(lit("CosineCurve"), QEasingCurve::CosineCurve);
    result->addItem(lit("BezierSpline"), QEasingCurve::BezierSpline);
    result->addItem(lit("TCBSpline"), QEasingCurve::TCBSpline);
    return result;
}

QString nameForId(Animations::Id id)
{
    switch (id)
    {
        case Animations::Id::TimelineExpand: return lit("TimelineExpand");
        case Animations::Id::TimelineCollapse: return lit("TimelineCollapse");
        case Animations::Id::TimelineShow: return lit("TimelineShow");
        case Animations::Id::TimelineHide: return lit("TimelineHide");
        case Animations::Id::TimelineTooltipShow: return lit("TimelineTooltipShow");
        case Animations::Id::TimelineTooltipHide: return lit("TimelineTooltipHide");
        case Animations::Id::TimelineButtonsShow: return lit("TimelineButtonsShow");
        case Animations::Id::TimelineButtonsHide: return lit("TimelineButtonsHide");
        case Animations::Id::ResourcesPanelExpand: return lit("ResourcesPanelExpand");
        case Animations::Id::ResourcesPanelCollapse: return lit("ResourcesPanelCollapse");
        case Animations::Id::ResourcesPanelTooltipShow: return lit("ResourcesPanelTooltipShow");
        case Animations::Id::ResourcesPanelTooltipHide: return lit("ResourcesPanelTooltipHide");
        case Animations::Id::NotificationsPanelExpand: return lit("NotificationsPanelExpand");
        case Animations::Id::NotificationsPanelCollapse: return lit("NotificationsPanelCollapse");
        case Animations::Id::TitlePanelExpand: return lit("TitlePanelExpand");
        case Animations::Id::TitlePanelCollapse: return lit("TitlePanelCollapse");
        case Animations::Id::CalendarShow: return lit("CalendarShow");
        case Animations::Id::CalendarHide: return lit("CalendarHide");
        case Animations::Id::SceneItemGeometryChange: return lit("SceneItemGeometryChange");
        case Animations::Id::SceneZoomIn: return lit("SceneZoomIn");
        case Animations::Id::SceneZoomOut: return lit("SceneZoomOut");
        case Animations::Id::ItemOverlayShow: return lit("ItemOverlayShow");
        case Animations::Id::ItemOverlayHide: return lit("ItemOverlayHide");
        default:
            break;
    }
    return QString();
}


QWidget* createWidget(QWidget* parent, Animations::Id id)
{
    QWidget* result = new QWidget(parent);
    QHBoxLayout* layout = new QHBoxLayout(result);
    layout->setContentsMargins(0,0,0,0);

    auto label = new QLabel(nameForId(id), result);
    layout->addWidget(label, 1);

    auto box = createEasingComboBox(result);
    layout->addWidget(box);
    box->setCurrentIndex(qnWorkbenchAnimations->easing(id));
    QObject::connect(box, QnComboboxCurrentIndexChanged,
        [id](int index)
        {
            qnWorkbenchAnimations->setEasing(id, static_cast<QEasingCurve::Type>(index));
        });

    auto timeEdit = new QSpinBox(result);
    timeEdit->setMinimum(1);
    timeEdit->setMaximum(9999);
    timeEdit->setValue(qnWorkbenchAnimations->timeLimit(id));
    QObject::connect(timeEdit, QnSpinboxIntValueChanged,
        [id](int value)
        {
            qnWorkbenchAnimations->setTimeLimit(id, value);
        });
    layout->addWidget(timeEdit);

    return result;
}

} // namespace


AnimationsControlDialog::AnimationsControlDialog(QWidget* parent):
    base_type(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(4);

    for (int i = 0; i < static_cast<int>(Animations::Id::IdCount); ++i)
    {
        Animations::Id id = static_cast<Animations::Id>(i);
        auto w = createWidget(this, id);
        layout->addWidget(w);
    }

    //layout->addStretch();
    setMinimumHeight(500);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

