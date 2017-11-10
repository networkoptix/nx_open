#pragma once

#include <core/resource/camera_advanced_param.h>
#include <nx/utils/log/assert.h>

class QWidget;
class QHBoxLayout;

namespace nx {
namespace client {
namespace desktop {

class AbstractCameraAdvancedParamWidget: public QWidget
{
    Q_OBJECT
public:
    explicit AbstractCameraAdvancedParamWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent);

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

class QnCameraAdvancedParamWidgetFactory
{
public:
    static AbstractCameraAdvancedParamWidget* createWidget(const QnCameraAdvancedParameter &parameter, QWidget* parent = NULL);
};

} // namespace desktop
} // namespace client
} // namespace nx