// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QOpenGLWidget>

#include <core/resource/resource_fwd.h>

class QnResourceDisplay;
class QnResourceWidgetRenderer;
typedef QSharedPointer<QnResourceDisplay> QnResourceDisplayPtr;

/**
 * Widget for displaying video from the given resource without constructing
 * the heavy graphics scene machinery.
 * Currently used in login dialog only and should die some day.
 */
class QnRenderingWidget: public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit QnRenderingWidget(QWidget* parent = nullptr, Qt::WindowFlags flags = {});

    virtual ~QnRenderingWidget() override;

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
    QnResourceWidgetRenderer* m_renderer = nullptr;
    QSize m_channelScreenSize;
    int m_effectiveWidth = 0;
};
