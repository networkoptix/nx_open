#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/camera_advanced_param.h>
#include <nx/utils/log/assert.h>

class QWidget;
class QHBoxLayout;

namespace nx::vms::client::desktop {

class AbstractCameraAdvancedParamWidget: public QWidget
{
    Q_OBJECT
public:
    explicit AbstractCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent);

    virtual QString value() const = 0;
    virtual void setValue(const QString &newValue) = 0;
    virtual QStringList range() const;
    virtual void setRange(const QString& range);

signals:
    void valueChanged(const QString &id, const QString &value);
protected:
    QString m_id;
    QHBoxLayout* m_layout;
};

class QnCameraAdvancedParamWidgetFactory
{
public:
    static AbstractCameraAdvancedParamWidget* createWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent = NULL);
};

} // namespace nx::vms::client::desktop
