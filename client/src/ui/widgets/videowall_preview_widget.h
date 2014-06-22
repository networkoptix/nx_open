#ifndef VIDEOWALL_PREVIEW_WIDGET_H
#define VIDEOWALL_PREVIEW_WIDGET_H

#include <QtWidgets/QWidget>

#include <client/client_model_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/common/geometry.h>

#include <ui/processors/drag_process_handler.h>

class DragProcessor;
class QAbstractAnimation;

class QnVideowallModel;

class QnVideowallPreviewWidget : public QWidget, public DragProcessHandler, private QnGeometry {
    Q_OBJECT
    typedef QWidget base_type;
public:
    explicit QnVideowallPreviewWidget(QWidget *parent = 0);
    virtual ~QnVideowallPreviewWidget();

    void loadFromResource(const QnVideoWallResourcePtr &videowall);
    void submitToResource(const QnVideoWallResourcePtr &videowall); 

protected:
    virtual void paintEvent(QPaintEvent *event) override;

    virtual bool eventFilter(QObject *target, QEvent *event) override;

    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    
private:
    void updateModel();

    void paintScreenFrame(QPainter *painter, BaseModelItem &item);
    void paintPlaceholder(QPainter* painter, BaseModelItem &item);

private:
    QScopedPointer<QnVideowallModel> m_model;
};

#endif // VIDEOWALL_PREVIEW_WIDGET_H
