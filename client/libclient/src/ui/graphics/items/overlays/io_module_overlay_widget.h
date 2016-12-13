#pragma once

#include <api/model/api_ioport_data.h>
#include <client/client_color_types.h>
#include <core/resource/resource_fwd.h>
#include <ui/graphics/items/standard/graphics_widget.h>

class QnIoModuleOverlayWidgetPrivate;
class QnIoModuleOverlayContents;

/*
Resource overlay providing user interaction with I/O module
*/
class QnIoModuleOverlayWidget: public GraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(QnIoModuleColors colors READ colors WRITE setColors)

    using base_type = GraphicsWidget;

public:
    QnIoModuleOverlayWidget(QGraphicsWidget* parent = nullptr);
    virtual ~QnIoModuleOverlayWidget();

    void setIOModule(const QnVirtualCameraResourcePtr& module);

    //TODO: Probably move to QnIoModuleOverlayContents or its descendants: */
    const QnIoModuleColors& colors() const;
    void setColors(const QnIoModuleColors& colors);

    /** Whether user is allowed to toggle output ports: */
    bool userInputEnabled() const;
    void setUserInputEnabled(bool value);

private:
    QScopedPointer<QnIoModuleOverlayWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnIoModuleOverlayWidget)
};

/*
Visual contents of I/O module overlay
*/
class QnIoModuleOverlayContents: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnIoModuleOverlayContents(QnIoModuleOverlayWidget* widget);
    virtual ~QnIoModuleOverlayContents();

    virtual void portsChanged(const QnIOPortDataList& ports) = 0;
    virtual void stateChanged(const QnIOPortData& port, const QnIOStateData& state) = 0;

    QnIoModuleOverlayWidget* widget() const;
    void setWidget(QnIoModuleOverlayWidget* widget);

signals:
    void userClicked(const QString& port);

private:
    QnIoModuleOverlayWidget* m_widget;
};
