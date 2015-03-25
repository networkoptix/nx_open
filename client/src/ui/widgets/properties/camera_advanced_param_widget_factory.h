#ifndef QN_CAMERA_ADVANCED_PARAM_WIDGET_FACTORY_H
#define QN_CAMERA_ADVANCED_PARAM_WIDGET_FACTORY_H

#include <core/resource/camera_advanced_param.h>

class QnAbstractCameraAdvancedParamWidget: public QWidget {
	Q_OBJECT
public:
	explicit QnAbstractCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent);

	virtual QString value() const = 0;
	virtual void setValue(const QString &newValue) = 0;
signals:
	void valueChanged(const QString &id, const QString &value);
protected:
	QString m_id;
	QHBoxLayout* m_layout;
};

class QnCameraAdvancedParamWidgetFactory {
public:
	static QnAbstractCameraAdvancedParamWidget* createWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent = NULL);
};

#endif //QN_CAMERA_ADVANCED_PARAM_WIDGET_FACTORY_H