#include "camera_motionmask_widget.h"

#include "ui/graphics/items/resource_widget.h"
#include "ui/workbench/workbench_item.h"
#include "ui/graphicsview.h"

QnCameraMotionMaskWidget::QnCameraMotionMaskWidget(const QString& unicId, QWidget* parent): QWidget(parent)
{
    m_widget = new QnResourceWidget(new QnWorkbenchItem(unicId), 0);
    m_widget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true); /* Optimization. */
    m_widget->setEnclosingGeometry(geometry());
    m_widget->setFrameWidth(0);

    m_scene.addItem(m_widget);
    
    m_view = new QGraphicsView(&m_scene);
    QGLFormat glFormat;
    glFormat.setOption(QGL::SampleBuffers); /* Multisampling. */
    glFormat.setSwapInterval(1); /* Turn vsync on. */
    m_view->setViewport(new QGLWidget(glFormat));

    //m_view->fitInView(m_widget, Qt::KeepAspectRatio);
    //m_view->show();

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(m_view);

    setLayout(layout);
}

QnCameraMotionMaskWidget::~QnCameraMotionMaskWidget()
{
    //delete m_view;
}


void QnCameraMotionMaskWidget::resizeEvent(QResizeEvent* event)
{
    //m_view->setGeometry(geometry());
    m_widget->setEnclosingGeometry(geometry());
}
