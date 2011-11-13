#ifndef QN_DRAG_PROCESSOR_H
#define QN_DRAG_PROCESSOR_H

#include <QObject>

class QWidget;
class QMouseEvent;

class DragProcessor;

class DragProcessListener {
public:
    DragProcessListener();
    virtual ~DragProcessListener();

protected:
    virtual void dragProcessStarted() {};
    virtual void dragStarted() {};
    virtual void drag() {};
    virtual void dragFinished() {};
    virtual void dragProcessFinished() {};

    DragProcessor *processor() const {
        return m_processor;
    }

private:
    friend class DragProcessor;

    DragProcessor *m_processor;
};


class DragProcessor: public QObject {
    Q_OBJECT;

public:
    DragProcessor(QObject *parent = NULL);

    virtual ~DragProcessor();

    DragProcessListener *listener() const {
        return m_listener;
    }

    void setListener(DragProcessListener *listener);

    void mousePressEvent(QWidget *viewport, QMouseEvent *event);
    void mouseMoveEvent(QWidget *viewport, QMouseEvent *event);
    void mouseReleaseEvent(QWidget *viewport, QMouseEvent *event);

private:
    DragProcessListener *m_listener;
};


#endif // QN_DRAG_PROCESSOR_H
