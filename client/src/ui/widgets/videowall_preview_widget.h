#ifndef VIDEOWALL_PREVIEW_WIDGET_H
#define VIDEOWALL_PREVIEW_WIDGET_H

#include <QtWidgets/QWidget>

#include <client/client_model_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/processors/drag_process_handler.h>

class DragProcessor;
class QAbstractAnimation;

class QnVideowallPreviewWidget : public QWidget, public DragProcessHandler {
    Q_OBJECT
    typedef QWidget base_type;
public:
    explicit QnVideowallPreviewWidget(QWidget *parent = 0);
    virtual ~QnVideowallPreviewWidget();

    QnVideoWallResourcePtr videowall() const;
    void setVideowall(const QnVideoWallResourcePtr &videowall);

    bool autoFill() const;
    void setAutoFill(bool value);

protected:
    virtual void paintEvent(QPaintEvent *event) override;

private:
    void updateModel();

private:
    QnVideoWallResourcePtr m_videowall;
    bool m_autoFill;
};

#endif // VIDEOWALL_PREVIEW_WIDGET_H
