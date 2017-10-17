#include "camera_advanced_param_widget_factory.h"

#include <QtCore/QObject>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>

#include <ui/common/read_only.h>
#include <ui/style/custom_style.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/utils/math/fuzzy.h>

QnAbstractCameraAdvancedParamWidget::QnAbstractCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent) :
    QWidget(parent),
    m_id(parameter.id),
    m_layout(new QHBoxLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);
}

class QnBoolCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget {
public:
    QnBoolCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
        QnAbstractCameraAdvancedParamWidget(parameter, parent),
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

class QnMinMaxStepCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget
{
public:
    QnMinMaxStepCameraAdvancedParamWidget(const QnCameraAdvancedParameter& parameter, QWidget* parent):
        QnAbstractCameraAdvancedParamWidget(parameter, parent),
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

class QnEnumerationCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget
{
public:
    QnEnumerationCameraAdvancedParamWidget(const QnCameraAdvancedParameter& parameter, QWidget* parent):
        QnAbstractCameraAdvancedParamWidget(parameter, parent),
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

    virtual void setRange(const QString& range)
    {
        auto rangeToSet = range.split(L',');
        setRange(rangeToSet);
    }

    void setRange(const QStringList& range)
    {
        m_comboBox->clear();
        m_comboBox->addItems(range);
    }

    virtual QString value() const override
    {
        return m_comboBox->currentText();
    }

    virtual void setValue(const QString& newValue) override
    {
        m_comboBox->setCurrentText(newValue);
    }

private:
    QComboBox* const m_comboBox = nullptr;
};

class QnButtonCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget {
public:
    QnButtonCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
        QnAbstractCameraAdvancedParamWidget(parameter, parent)
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

class QnStringCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget {
public:
    QnStringCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
        QnAbstractCameraAdvancedParamWidget(parameter, parent),
        m_lineEdit(new QLineEdit(this))
    {
        m_lineEdit->setToolTip(parameter.description);
        setReadOnly(m_lineEdit, parameter.readOnly);

        m_layout->insertWidget(0, m_lineEdit, 1);

        connect(m_lineEdit, &QLineEdit::textEdited, this, [this] {
            emit valueChanged(m_id, value());
        });
    }

    virtual QString value() const override	{
        return m_lineEdit->text();
    }

    virtual void setValue(const QString &newValue) override	{
        m_lineEdit->setText(newValue);
    }

    virtual QSize sizeHint() const override
    {
        // TODO: #GDM Looks like dirty hack. Investigation is required. #low #future
        return QSize(9999, m_lineEdit->sizeHint().height());
    }

private:
    QLineEdit* m_lineEdit;
};

class QnSeparatorCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget
{
public:
    QnSeparatorCameraAdvancedParamWidget(const QnCameraAdvancedParameter& parameter, QWidget* parent):
        QnAbstractCameraAdvancedParamWidget(parameter, parent),
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

QnAbstractCameraAdvancedParamWidget* QnCameraAdvancedParamWidgetFactory::createWidget(
    const QnCameraAdvancedParameter& parameter, QWidget* parent)
{
    if (!parameter.isValid())
        return nullptr;

    switch (parameter.dataType)
    {
        /* CheckBox */
        case QnCameraAdvancedParameter::DataType::Bool:
            return new QnBoolCameraAdvancedParamWidget(parameter, parent);

        /* Slider */
        case QnCameraAdvancedParameter::DataType::Number:
            return new QnMinMaxStepCameraAdvancedParamWidget(parameter, parent);

        /* Drop-down box. */
        case QnCameraAdvancedParameter::DataType::Enumeration:
            return new QnEnumerationCameraAdvancedParamWidget(parameter, parent);

        /* Button */
        case QnCameraAdvancedParameter::DataType::Button:
            return new QnButtonCameraAdvancedParamWidget(parameter, parent);

        /* LineEdit  */
        case QnCameraAdvancedParameter::DataType::String:
            return new QnStringCameraAdvancedParamWidget(parameter, parent);

        /* Separator */
        case QnCameraAdvancedParameter::DataType::Separator:
            return new QnSeparatorCameraAdvancedParamWidget(parameter, parent);

        default:
            return nullptr;
    }
}


