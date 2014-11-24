#include "camera_advanced_param_widget_factory.h"

#include <ui/common/read_only.h>
#include <ui/style/warning_style.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <QtCore/QObject>

QnAbstractCameraAdvancedParamWidget::QnAbstractCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
	QWidget(parent),
	m_id(parameter.getId()),
	m_layout(new QHBoxLayout(this))
{
	m_layout->setContentsMargins(0, 0, 0, 0);
	setLayout(m_layout);

	if (parameter.readOnly) {
		QLabel* readOnlyLabel = new QLabel(tr("Read-Only"));
		setWarningStyle(readOnlyLabel);
		m_layout->addWidget(readOnlyLabel);
	}
		
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
		m_checkBox->setChecked(newValue == lit("true"));
	}

private:
	QCheckBox* m_checkBox;
};

class QnMinMaxStepCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget {
public:
	QnMinMaxStepCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
		QnAbstractCameraAdvancedParamWidget(parameter, parent),
		m_spinBox(new QSpinBox(this))
	{
		m_spinBox->setMinimum(parameter.min.toInt());
		m_spinBox->setMaximum(parameter.max.toInt());
		m_spinBox->setSingleStep(parameter.step.toInt());
		m_spinBox->setToolTip(parameter.description);
		setReadOnly(m_spinBox, parameter.readOnly);

		m_layout->insertStretch(0);
		m_layout->insertWidget(0, m_spinBox);
		connect(m_spinBox, QnSpinboxIntValueChanged, this, [this] {
			emit valueChanged(m_id, value());
		});
	}

	virtual QString value() const override	{
		return QString::number(m_spinBox->value());
	}

	virtual void setValue(const QString &newValue) override	{
		m_spinBox->setValue(newValue.toInt());
	}

private:
	QSpinBox* m_spinBox;
};

class QnEnumerationCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget {
public:
	QnEnumerationCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
		QnAbstractCameraAdvancedParamWidget(parameter, parent),
		m_comboBox(new QComboBox(this))
	{
		m_comboBox->addItems(parameter.min.split(L',', QString::SkipEmptyParts));
		m_comboBox->setToolTip(parameter.description);
		setReadOnly(m_comboBox, parameter.readOnly);

		m_layout->insertStretch(0);
		m_layout->insertWidget(0, m_comboBox);
		connect(m_comboBox, &QComboBox::currentTextChanged, this, [this] {
			emit valueChanged(m_id, value());
		});
	}

	virtual QString value() const override	{
		return QString::number(m_comboBox->currentIndex());
	}

	virtual void setValue(const QString &newValue) override	{
		m_comboBox->setCurrentIndex(newValue.toInt());
	}

private:
	QComboBox* m_comboBox;
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
		connect(button, &QPushButton::clicked, this, [this] {
			emit valueChanged(m_id, QString());
		});
	}

	virtual QString value() const override	{ return QString(); }
	virtual void setValue(const QString &newValue) override	{}
};

class QnStringCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget {
public:
	QnStringCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
		QnAbstractCameraAdvancedParamWidget(parameter, parent),
		m_lineEdit(new QLineEdit(this))
	{
		m_lineEdit->setToolTip(parameter.description);
		setReadOnly(m_lineEdit, parameter.readOnly);

		m_layout->insertWidget(0, m_lineEdit);
		
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

private:
	QLineEdit* m_lineEdit;
};

class QnControlButtonsPairCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget {
public:
	QnControlButtonsPairCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
		QnAbstractCameraAdvancedParamWidget(parameter, parent)
	{
		QPushButton *plusButton = new QPushButton(this);
		plusButton->setText(lit("+"));
		plusButton->setToolTip(parameter.description);
		setReadOnly(plusButton, parameter.readOnly);

		QPushButton *minusButton = new QPushButton(this);
		minusButton->setText(lit("-"));
		minusButton->setToolTip(parameter.description);
		setReadOnly(minusButton, parameter.readOnly);

		m_layout->insertStretch(0);
		m_layout->insertWidget(0, minusButton);
		m_layout->insertWidget(0, plusButton);

		connect(plusButton, &QPushButton::clicked, this, [this] {
			emit valueChanged(m_id, QString());
		});

		connect(minusButton, &QPushButton::clicked, this, [this] {
			emit valueChanged(m_id, QString());
		});
	}

	virtual QString value() const override	{
		return QString();
	}

	virtual void setValue(const QString &newValue) override	{
		//do nothing
	}
};

QnAbstractCameraAdvancedParamWidget* QnCameraAdvancedParamWidgetFactory::createWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent) {
	if (!parameter.isValid())
		return NULL;

	switch (parameter.dataType) {

	/* CheckBox */
	case QnCameraAdvancedParameter::DataType::Bool:
		return new QnBoolCameraAdvancedParamWidget(parameter, parent);

	/* Slider */
	case QnCameraAdvancedParameter::DataType::MinMaxStep:
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

	/* Pair of buttons, one with the '+' sign, one with the '-'. */
	case QnCameraAdvancedParameter::DataType::ControlButtonsPair:
		return new QnControlButtonsPairCameraAdvancedParamWidget(parameter, parent);

	default:
		return NULL;
	}
}


