#ifndef QN_CAMERA_ADVANCED_PARAM_WIDGET_FACTORY_H
#define QN_CAMERA_ADVANCED_PARAM_WIDGET_FACTORY_H

#include <QtWidgets/QWidget>

#include <core/resource/camera_advanced_param.h>
#include <nx/utils/log/assert.h>

class QnAbstractCameraAdvancedParamWidget: public QWidget {
	Q_OBJECT
public:
	explicit QnAbstractCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent);

	virtual QString value() const = 0;
	virtual void setValue(const QString &newValue) = 0;
    virtual void setRange(const QString& /*range*/)
    {
        NX_ASSERT(false, lit("setRange allowed to be called only for Enumeration widget."));
    }

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
