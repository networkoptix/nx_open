#include "camera_advanced_param_widget_factory.h"

#include <QtCore/QObject>

QnAbstractCameraAdvancedParamWidget::QnAbstractCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
	QWidget(parent),
	m_id(parameter.getId()),
	m_layout(new QHBoxLayout(this))
{
	setLayout(m_layout);
}

class QnBoolCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget {
public:
	QnBoolCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
		QnAbstractCameraAdvancedParamWidget(parameter, parent),
		m_checkBox(new QCheckBox(this))
	{
		m_layout->addWidget(m_checkBox);
		m_layout->addStretch();
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
		m_checkBox(new QCheckBox(this))
	{
		m_layout->addWidget(m_checkBox);
		m_layout->addStretch();
		connect(m_checkBox, &QCheckBox::stateChanged, this, [this] {
			emit valueChanged(m_id, value());
		});
	}

	virtual QString value() const override	{
		return QString();
	}

	virtual void setValue(const QString &newValue) override	{
		
	}

private:
	QCheckBox* m_checkBox;
};

class QnEnumerationCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget {
public:
	QnEnumerationCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
		QnAbstractCameraAdvancedParamWidget(parameter, parent),
		m_checkBox(new QCheckBox(this))
	{
		m_layout->addWidget(m_checkBox);
		m_layout->addStretch();
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

class QnButtonCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget {
public:
	QnButtonCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
		QnAbstractCameraAdvancedParamWidget(parameter, parent)
	{
		QPushButton *button = new QPushButton(this);
		m_layout->addWidget(button);
		m_layout->addStretch();
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
		m_checkBox(new QCheckBox(this))
	{
		m_layout->addWidget(m_checkBox);
		m_layout->addStretch();
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

class QnControlButtonsPairCameraAdvancedParamWidget: public QnAbstractCameraAdvancedParamWidget {
public:
	QnControlButtonsPairCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent):
		QnAbstractCameraAdvancedParamWidget(parameter, parent),
		m_checkBox(new QCheckBox(this))
	{
		m_layout->addWidget(m_checkBox);
		m_layout->addStretch();
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

QnAbstractCameraAdvancedParamWidget* QnCameraAdvancedParamWidgetFactory::createWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent) {
	if (parameter.query.isNull())
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


