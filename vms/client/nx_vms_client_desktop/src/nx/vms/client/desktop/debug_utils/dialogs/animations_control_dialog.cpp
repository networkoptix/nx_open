// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "animations_control_dialog.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QBoxLayout>

#include <nx/reflect/to_string.h>
#include <nx/vms/client/desktop/workbench/workbench_animations.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>

#include "../utils/debug_custom_actions.h"

namespace nx::vms::client::desktop {

using namespace ui::workbench;

namespace {

QComboBox* createEasingComboBox(QWidget* parent)
{
    QComboBox* result = new QComboBox(parent);
    result->addItem("Linear", QEasingCurve::Linear);
    result->addItem("InQuad", QEasingCurve::InQuad);
    result->addItem("OutQuad", QEasingCurve::OutQuad);
    result->addItem("InOutQuad", QEasingCurve::InOutQuad);
    result->addItem("OutInQuad", QEasingCurve::OutInQuad);
    result->addItem("InCubic", QEasingCurve::InCubic);
    result->addItem("OutCubic", QEasingCurve::OutCubic);
    result->addItem("InOutCubic", QEasingCurve::InOutCubic);
    result->addItem("OutInCubic", QEasingCurve::OutInCubic);
    result->addItem("InQuart", QEasingCurve::InQuart);
    result->addItem("OutQuart", QEasingCurve::OutQuart);
    result->addItem("InOutQuart", QEasingCurve::InOutQuart);
    result->addItem("OutInQuart", QEasingCurve::OutInQuart);
    result->addItem("InQuint", QEasingCurve::InQuint);
    result->addItem("OutQuint", QEasingCurve::OutQuint);
    result->addItem("InOutQuint", QEasingCurve::InOutQuint);
    result->addItem("OutInQuint", QEasingCurve::OutInQuint);
    result->addItem("InSine", QEasingCurve::InSine);
    result->addItem("OutSine", QEasingCurve::OutSine);
    result->addItem("InOutSine", QEasingCurve::InOutSine);
    result->addItem("OutInSine", QEasingCurve::OutInSine);
    result->addItem("InExpo", QEasingCurve::InExpo);
    result->addItem("OutExpo", QEasingCurve::OutExpo);
    result->addItem("InOutExpo", QEasingCurve::InOutExpo);
    result->addItem("OutInExpo", QEasingCurve::OutInExpo);
    result->addItem("InCirc", QEasingCurve::InCirc);
    result->addItem("OutCirc", QEasingCurve::OutCirc);
    result->addItem("InOutCirc", QEasingCurve::InOutCirc);
    result->addItem("OutInCirc", QEasingCurve::OutInCirc);
    result->addItem("InElastic", QEasingCurve::InElastic);
    result->addItem("OutElastic", QEasingCurve::OutElastic);
    result->addItem("InOutElastic", QEasingCurve::InOutElastic);
    result->addItem("OutInElastic", QEasingCurve::OutInElastic);
    result->addItem("InBack", QEasingCurve::InBack);
    result->addItem("OutBack", QEasingCurve::OutBack);
    result->addItem("InOutBack", QEasingCurve::InOutBack);
    result->addItem("OutInBack", QEasingCurve::OutInBack);
    result->addItem("InBounce", QEasingCurve::InBounce);
    result->addItem("OutBounce", QEasingCurve::OutBounce);
    result->addItem("InOutBounce", QEasingCurve::InOutBounce);
    result->addItem("OutInBounce", QEasingCurve::OutInBounce);
    result->addItem("InCurve", QEasingCurve::InCurve);
    result->addItem("OutCurve", QEasingCurve::OutCurve);
    result->addItem("SineCurve", QEasingCurve::SineCurve);
    result->addItem("CosineCurve", QEasingCurve::CosineCurve);
    result->addItem("BezierSpline", QEasingCurve::BezierSpline);
    result->addItem("TCBSpline", QEasingCurve::TCBSpline);
    return result;
}

QString nameForId(Animations::Id id)
{
    return QString::fromStdString(nx::reflect::toString(id));
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

void AnimationsControlDialog::registerAction()
{
    registerDebugAction(
        "Animations",
        [](QnWorkbenchContext* context)
        {
            auto dialog = std::make_unique<AnimationsControlDialog>(context->mainWindowWidget());
            dialog->exec();
        });
}

} // namespace nx::vms::client::desktop
