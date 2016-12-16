#pragma once

#include <api/model/api_ioport_data.h>
#include <core/resource/resource_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <ui/customization/customized.h>
#include <ui/graphics/items/standard/graphics_widget.h>

class QnIoModuleOverlayWidgetPrivate;
class QnIoModuleOverlayContents;

/*
Resource overlay providing user interaction with I/O module
*/
class QnIoModuleOverlayWidget: public GraphicsWidget
{
    Q_OBJECT
    using base_type = GraphicsWidget;

public:
    QnIoModuleOverlayWidget(QGraphicsWidget* parent = nullptr);
    virtual ~QnIoModuleOverlayWidget();

    void setIOModule(const QnVirtualCameraResourcePtr& module);

    enum class Style
    {
        Form,
        Tile,
        Default = Form
    };

    /** Overlay style set in accordance with resource settings: */
    Style overlayStyle() const;

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
class QnIoModuleOverlayContents: public Customized<QGraphicsWidget>
{
    Q_OBJECT
    using base_type = Customized<QGraphicsWidget>;

public:
    QnIoModuleOverlayContents();
    virtual ~QnIoModuleOverlayContents();

    virtual void portsChanged(const QnIOPortDataList& ports, bool userInputEnabled) = 0;
    virtual void stateChanged(const QnIOPortData& port, const QnIOStateData& state) = 0;

    QnIoModuleOverlayWidget* overlayWidget() const;

signals:
    void userClicked(const QString& port);
};

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnIoModuleOverlayWidget::Style), (lexical))
