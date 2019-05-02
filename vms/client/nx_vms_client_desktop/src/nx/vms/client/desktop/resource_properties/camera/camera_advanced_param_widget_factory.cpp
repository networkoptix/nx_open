#include "camera_advanced_param_widget_factory.h"

#include <QtCore/QObject>

#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>
#include <qcoreapplication.h>   // for Q_DECLARE_TR_FUNCTIONS

#include <ui/common/read_only.h>
#include <ui/style/custom_style.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/desktop/common/widgets/lens_ptz_control.h>
#include <nx/vms/client/desktop/common/widgets/button_slider.h>
#include <nx/vms/client/desktop/common/widgets/hover_button.h>
#include <utils/common/scoped_value_rollback.h>

namespace nx::vms::client::desktop {

AbstractCameraAdvancedParamWidget::AbstractCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent) :
    QWidget(parent),
    m_id(parameter.id),
    m_layout(new QHBoxLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);
}

QStringList AbstractCameraAdvancedParamWidget::range() const
{
    NX_ASSERT(false, lit("range allowed to be called only for Enumeration widget."));
    return QStringList();
}

void AbstractCameraAdvancedParamWidget::setRange(const QString& /*range*/)
{
    NX_ASSERT(false, lit("setRange allowed to be called only for Enumeration widget."));
}

class QnBoolCameraAdvancedParamWidget: public AbstractCameraAdvancedParamWidget
{
public:
    QnBoolCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
        AbstractCameraAdvancedParamWidget(parameter, parent),
        m_checkBox(new QCheckBox(this))
    {
        m_checkBox->setToolTip(parameter.description);
        setReadOnly(m_checkBox, parameter.readOnly);

        m_layout->insertStretch(0);
        m_layout->insertWidget(0, m_checkBox);
        connect(m_checkBox, &QCheckBox::stateChanged, this, [this] {
            emit valueChanged(m_id, value());
        });
    }

    virtual QString value() const override	{
        return m_checkBox->isChecked() ? lit("true") : lit("false");
    }

    virtual void setValue(const QString &newValue) override	{
        m_checkBox->setChecked(newValue == lit("true") || newValue == lit("1"));
    }

private:
    QCheckBox* m_checkBox;
};

class QnMinMaxStepCameraAdvancedParamWidget: public AbstractCameraAdvancedParamWidget
{
public:
    QnMinMaxStepCameraAdvancedParamWidget(const QnCameraAdvancedParameter& parameter, QWidget* parent):
        AbstractCameraAdvancedParamWidget(parameter, parent),
        m_spinBox(new QSpinBox(this))
    {
        double min = 0;
        double max = 0;
        parameter.getRange(min, max);

        m_isInteger = qFuzzyEquals(qRound(min), min)
            && qFuzzyEquals(qRound(max), max)
            && (max - min >= 1.0);

        if (m_isInteger)
        {
            m_spinBox->setMinimum(qRound(min));
            m_spinBox->setMaximum(qRound(max));
        }
        else
        {
            m_spinBox->setMinimum(qRound(min * 100));
            m_spinBox->setMaximum(qRound(max * 100));
        }

        m_spinBox->setToolTip(parameter.description);
        setReadOnly(m_spinBox, parameter.readOnly);

        if (parameter.readOnly)
            m_spinBox->setButtonSymbols(QSpinBox::NoButtons);

        if (!parameter.unit.isEmpty())
            m_spinBox->setSuffix(lit(" %1").arg(parameter.unit));

        m_layout->addWidget(m_spinBox);
        m_layout->addStretch(0);

        connect(m_spinBox, QnSpinboxIntValueChanged, this,
            [this]()
            {
                emit valueChanged(m_id, value());
            });
    }

    virtual QString value() const override
    {
        int innerValue = m_spinBox->value();
        if (m_isInteger)
            return QString::number(innerValue);
        return QString::number(0.01 * innerValue);
    }

    virtual void setValue(const QString &newValue) override
    {
        if (m_isInteger)
            m_spinBox->setValue(newValue.toInt());
        else
            m_spinBox->setValue(qRound(newValue.toDouble() * 100));
    }

    virtual void setRange(const QString& range) override
    {
        const auto minMax = range.split(L',');
        if (minMax.size() != 2)
            return;

        bool success = false;
        double min = minMax[0].toDouble(&success);
        if (!success)
            return;

        double max = minMax[1].toDouble(&success);
        if (!success)
            return;

        m_isInteger = qFuzzyEquals(qRound(min), min)
            && qFuzzyEquals(qRound(max), max)
            && (max - min >= 1.0);

        if (m_isInteger)
        {
            m_spinBox->setMinimum(qRound(min));
            m_spinBox->setMaximum(qRound(max));
        }
        else
        {
            m_spinBox->setMinimum(qRound(min * 100));
            m_spinBox->setMaximum(qRound(max * 100));
        }
    }

private:
    QSpinBox* const m_spinBox = nullptr;
    bool m_isInteger = false;
};

class QnEnumerationCameraAdvancedParamWidget: public AbstractCameraAdvancedParamWidget
{
public:
    QnEnumerationCameraAdvancedParamWidget(const QnCameraAdvancedParameter& parameter, QWidget* parent):
        AbstractCameraAdvancedParamWidget(parameter, parent),
        m_comboBox(new QComboBox(this))
    {
        m_comboBox->addItems(parameter.getRange());
        m_comboBox->setToolTip(parameter.description);
        setReadOnly(m_comboBox, parameter.readOnly);

        m_layout->addWidget(m_comboBox);
        connect(m_comboBox, &QComboBox::currentTextChanged, this,
            [this]()
            {
                emit valueChanged(m_id, value());
            });
    }

    virtual void setRange(const QString& range) override
    {
        auto rangeToSet = range.split(L',');
        setRange(rangeToSet);
    }

    virtual QStringList range() const override
    {
        QStringList result;
        for (auto i = 0; i < m_comboBox->count(); ++i)
            result << m_comboBox->itemText(i);

        return result;
    }

    void setRange(const QStringList& range)
    {
        const QString previousValue = m_comboBox->currentText();
        m_comboBox->clear();
        m_comboBox->addItems(range);
        if (m_comboBox->findText(previousValue) != -1)
            setValue(previousValue);
    }

    virtual QString value() const override
    {
        return m_comboBox->currentText();
    }

    virtual void setValue(const QString& newValue) override
    {
        if (m_comboBox->findText(newValue) == -1)
            return;
        m_comboBox->setCurrentText(newValue);
    }

private:
    QComboBox* const m_comboBox = nullptr;
};

class QnButtonCameraAdvancedParamWidget: public AbstractCameraAdvancedParamWidget
{
public:
    QnButtonCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
        AbstractCameraAdvancedParamWidget(parameter, parent)
    {
        QPushButton *button = new QPushButton(this);
        button->setText(parameter.name);
        button->setToolTip(parameter.description);
        setReadOnly(button, parameter.readOnly);

        m_layout->insertStretch(0);
        m_layout->insertWidget(0, button);
        connect(button, &QPushButton::clicked, this,
            [this, confirmation = parameter.confirmation]
            {
                if (!confirmation.isEmpty())
                {
                    auto result = QnMessageBox::question(
                        this,
                        confirmation,
                        QString(),
                        QDialogButtonBox::Yes | QDialogButtonBox::Cancel);

                    if (result != QDialogButtonBox::Yes)
                        return;
                }
                emit valueChanged(m_id, QString());
            });
    }

    virtual QString value() const override	{ return QString(); }
    virtual void setValue(const QString &newValue) override	{ Q_UNUSED(newValue); }
};

template<typename TextControl>
class QnTextControlCameraAdvancedParamWidget: public AbstractCameraAdvancedParamWidget
{
    static QString getText(QLineEdit* control)
    {
        return control->text();
    }

    static QString getText(QTextEdit* control)
    {
        return control->toPlainText();
    }

public:
    QnTextControlCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
        AbstractCameraAdvancedParamWidget(parameter, parent),
        m_textControl(new TextControl(this))
    {
        m_textControl->setToolTip(parameter.description);
        setReadOnly(m_textControl, parameter.readOnly);

        m_layout->insertWidget(0, m_textControl, 1);

        connect(m_textControl, &TextControl::textChanged, this,
            [this]()
            {
                if (!m_updateInProgress)
                    emit valueChanged(m_id, value());
            });
    }

    virtual QString value() const override
    {
        return getText(m_textControl);
    }

    virtual void setValue(const QString &newValue) override
    {
        const QnScopedValueRollback rollback(&m_updateInProgress, true);
        m_textControl->setText(newValue);
    }

    virtual QSize sizeHint() const override
    {
        // TODO: #GDM Looks like dirty hack. Investigation is required. #low #future
        return QSize(9999, m_textControl->sizeHint().height());
    }

private:
    TextControl* m_textControl = nullptr;
    bool m_updateInProgress = false;
};

// Wrapper for a vertical slider.
class QnVSliderCameraAdvancedParamWidget: public AbstractCameraAdvancedParamWidget
{
    Q_DECLARE_TR_FUNCTIONS(QnVSliderCameraAdvancedParamWidget)

public:
    QnVSliderCameraAdvancedParamWidget(const QnCameraAdvancedParameter& parameter, QWidget* parent) :
        AbstractCameraAdvancedParamWidget(parameter, parent)
    {
        m_slider = new nx::vms::client::desktop::VButtonSlider(this);
        m_slider->setText(parameter.name);
        m_slider->setMaximumWidth(80);
        m_layout->insertWidget(0, m_slider);

        connect(m_slider, &nx::vms::client::desktop::VButtonSlider::valueChanged, this,
            [this](int val)
            {
                emit valueChanged(m_id, value());
            });
        // Hardcoding its position and range for now.
        m_slider->setMaximum(100);
        m_slider->setMinimum(-100);
        m_slider->setButtonIncrement(50);
        m_slider->setSliderPosition(0);
    }

    virtual QString value() const override
    {
        int innerValue = m_slider->sliderPosition();
        return QString::number(innerValue);
    }

    virtual void setValue(const QString &newValue) override
    {
        int intValue = newValue.toInt();
        m_slider->setSliderPosition(intValue);
    }

    virtual void setRange(const QString& range) override
    {
        QStringList minMax = range.split(L',');
        if (minMax.size() != 2)
            return;

        bool success = false;
        int min = minMax[0].toInt(&success);
        if (!success)
            return;

        int max = minMax[1].toInt(&success);
        if (!success)
            return;

        m_slider->setMinimum(min);
        m_slider->setMaximum(max);
    }

    virtual QSize sizeHint() const override
    {
        return m_slider->sizeHint();
    }

    nx::vms::client::desktop::VButtonSlider* m_slider = nullptr;
};

class QnPanTiltRotationCameraAdvancedParamWidget : public AbstractCameraAdvancedParamWidget
{
    Q_DECLARE_TR_FUNCTIONS(QnPanTiltRotationCameraAdvancedParamWidget)
    using LensPtzControl = nx::vms::client::desktop::LensPtzControl;

public:
    QnPanTiltRotationCameraAdvancedParamWidget(const QnCameraAdvancedParameter& parameter, QWidget* parent):
        AbstractCameraAdvancedParamWidget(parameter, parent),
        m_ptrWidget(new LensPtzControl(this))
    {
        QSize buttonSize(30, 30);

        const QString kIconCw(lit("text_buttons/rotate_cw.png"));
        const QString kIconCwHovered(lit("text_buttons/rotate_cw_hovered.png"));
        const QString kIconCcw(lit("text_buttons/rotate_ccw.png"));
        const QString kIconCcwHovered(lit("text_buttons/rotate_ccw_hovered.png"));

        // Central widget is here.
        const auto ptzrContainer = new QVBoxLayout();
        ptzrContainer->addWidget(m_ptrWidget);

        const auto ptzrInfoContainer = new QHBoxLayout();

        m_rotationCw = new nx::vms::client::desktop::HoverButton(kIconCw, kIconCwHovered, this);
        m_rotationCcw = new nx::vms::client::desktop::HoverButton(kIconCcw, kIconCcwHovered, this);
        m_rotationLabel = new QLabel();
        m_rotationLabel->setText(rotationText(0));
        ptzrInfoContainer->addWidget(m_rotationCw);
        ptzrInfoContainer->addWidget(m_rotationLabel);
        ptzrInfoContainer->addWidget(m_rotationCcw);
        ptzrContainer->addLayout(ptzrInfoContainer);
        ptzrContainer->setAlignment(ptzrInfoContainer, Qt::AlignCenter);

        m_layout->addLayout(ptzrContainer);

        connect(m_rotationCcw, &QAbstractButton::pressed,
            this, [this]() { onRotationCcw(true); });
        connect(m_rotationCcw, &QAbstractButton::released,
            this, [this]() { onRotationCcw(false); });

        connect(m_rotationCw, &QAbstractButton::pressed,
            this, [this]() { onRotationCw(true); });
        connect(m_rotationCw, &QAbstractButton::released,
            this, [this]() { onRotationCw(false); });

        connect(m_ptrWidget, &LensPtzControl::valueChanged, this,
            [this](const LensPtzControl::Value& v)
            {
                m_rotationLabel->setText(rotationText(v.rotation));
                emit valueChanged(m_id, value());
            });
    }

    void onRotationCcw(bool value)
    {
        m_ptrWidget->onRotationButtonCounterClockWise(value);
    }

    void onRotationCw(bool value)
    {
        m_ptrWidget->onRotationButtonClockWise(value);
    }

    QString rotationText(int rotation)
    {
        // Right now we do not have specific rotation. So we disable it for now.
        //return tr("Rotation: ") + QString::number(int(rotation)) + lit("\xB0");
        return tr("Rotation");
    }

    virtual QString value() const override
    {
        auto val = m_ptrWidget->value();
        return lit("%1,%2,%3").arg(val.horizontal).arg(val.vertical).arg(val.rotation);
    }

    virtual void setRange(const QString& range) override
    {
        // Expecting 6 numbers, like "-40,40,-30,30,-40,40";
        qDebug() << "QnPanTiltRotationCameraAdvancedParamWidget setRange(" << range << ")";
        QStringList minMax = range.split(L',');
        if (minMax.size() != 6)
            return;
    }

    virtual void setValue(const QString &range) override
    {
        qDebug() << "QnPanTiltRotationCameraAdvancedParamWidget setValue(" << range << ")";
        // Expecting 3 numbers, like "4,-3,3";
        QStringList minMax = range.split(L',');
        if (minMax.size() != 3)
            return;
        // TODO: Impelemt it, when a proper value infrastructure implemented for this case.
    }

protected:
    nx::vms::client::desktop::LensPtzControl* m_ptrWidget = nullptr;
    QAbstractButton* m_rotationCcw = nullptr;
    QAbstractButton* m_rotationCw = nullptr;
    QLabel* m_rotationLabel = nullptr;
};

class QnSeparatorCameraAdvancedParamWidget: public AbstractCameraAdvancedParamWidget
{
public:
    QnSeparatorCameraAdvancedParamWidget(const QnCameraAdvancedParameter& parameter, QWidget* parent):
        AbstractCameraAdvancedParamWidget(parameter, parent),
        m_line(new QFrame(this))
    {
        m_line->setFrameShape(QFrame::HLine);
        m_line->setFrameShadow(QFrame::Sunken);
        m_layout->addWidget(m_line);
    }

    virtual QString value() const override { return QString(); }
    virtual void setValue(const QString& /*newValue*/) override {}

private:
    QFrame* const m_line = nullptr;
};

AbstractCameraAdvancedParamWidget* QnCameraAdvancedParamWidgetFactory::createWidget(
    const QnCameraAdvancedParameter& parameter, QWidget* parent)
{
    if (!parameter.isValid())
        return nullptr;

    switch (parameter.dataType)
    {
        // CheckBox.
        case QnCameraAdvancedParameter::DataType::Bool:
            return new QnBoolCameraAdvancedParamWidget(parameter, parent);

        // Slider.
        case QnCameraAdvancedParameter::DataType::Number:
            return new QnMinMaxStepCameraAdvancedParamWidget(parameter, parent);

        // Drop-down box.
        case QnCameraAdvancedParameter::DataType::Enumeration:
            return new QnEnumerationCameraAdvancedParamWidget(parameter, parent);

        // Button
        case QnCameraAdvancedParameter::DataType::Button:
            return new QnButtonCameraAdvancedParamWidget(parameter, parent);

        // LineEdit.
        case QnCameraAdvancedParameter::DataType::String:
            return new QnTextControlCameraAdvancedParamWidget<QLineEdit>(parameter, parent);

        // TextEdit
        case QnCameraAdvancedParameter::DataType::Text:
            return new QnTextControlCameraAdvancedParamWidget<QTextEdit>(parameter, parent);

        // Separator.
        case QnCameraAdvancedParameter::DataType::Separator:
            return new QnSeparatorCameraAdvancedParamWidget(parameter, parent);

        // Vertical slider.
        case QnCameraAdvancedParameter::DataType::SliderControl:
            return new QnVSliderCameraAdvancedParamWidget(parameter, parent);

        // Round ptr control.
        case QnCameraAdvancedParameter::DataType::PtrControl:
            return new QnPanTiltRotationCameraAdvancedParamWidget(parameter, parent);

        default:
            return nullptr;
    }
}

} // namespace nx::vms::client::desktop
