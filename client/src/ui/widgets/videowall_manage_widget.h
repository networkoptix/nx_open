#ifndef VIDEOWALL_MANAGE_WIDGET_H
#define VIDEOWALL_MANAGE_WIDGET_H

#include <QtWidgets/QWidget>

#include <client/client_model_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/processors/drag_process_handler.h>

class DragProcessor;
class QAbstractAnimation;

class QnVideowallManageWidgetPrivate;

class QnVideowallManageWidget : public QWidget, public DragProcessHandler {
    Q_OBJECT
    typedef QWidget base_type;
public:
    explicit QnVideowallManageWidget(QWidget *parent = 0);
    virtual ~QnVideowallManageWidget();

    void loadFromResource(const QnVideoWallResourcePtr &videowall);
    void submitToResource(const QnVideoWallResourcePtr &videowall); 

protected:
    Q_DECLARE_PRIVATE(QnVideowallManageWidget);

    virtual void paintEvent(QPaintEvent *event) override;

    virtual bool eventFilter(QObject *target, QEvent *event) override;

    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
private:
    QScopedPointer<QnVideowallManageWidgetPrivate> d_ptr;
};

#endif // VIDEOWALL_MANAGE_WIDGET_H
