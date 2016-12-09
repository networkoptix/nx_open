#pragma once

#include <QtOpenGL/QGLWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workaround/gl_widget_workaround.h>

class QnResourceDisplay;
class QnResourceWidgetRenderer;
typedef QSharedPointer<QnResourceDisplay> QnResourceDisplayPtr;

/**
 * Widget for displaying video from the given resource without constructing
 * the heavy graphics scene machinery.
 */
class QnRenderingWidget: public QnGLWidget
{
    Q_OBJECT

public:
    explicit QnRenderingWidget(const QGLFormat& format, QWidget* parent = nullptr,
        QGLWidget* shareWidget = nullptr, Qt::WindowFlags f = 0);
    virtual ~QnRenderingWidget();

    QnMediaResourcePtr resource() const;
    void setResource(const QnMediaResourcePtr& resource);

    void stopPlayback();

    void setEffectiveWidth(int value);
    int effectiveWidth() const;

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int width) const override;

protected:
    void updateChannelScreenSize();

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int width, int height) override;

    void invalidateDisplay();
    void ensureDisplay();

private:
    QnMediaResourcePtr m_resource;
    QnResourceDisplayPtr m_display;
    QnResourceWidgetRenderer* m_renderer;
    QSize m_channelScreenSize;
    int m_effectiveWidth = 0;
};
