class HardwareCheckEventFilter: public QObject{
private:
    const QGLContext* m_context;
public:
    HardwareCheckEventFilter(QGLWidget* parent):
        QObject(parent),
        m_context(parent->context()){
            parent->installEventFilter(this);
        }
    virtual ~HardwareCheckEventFilter();
    virtual bool eventFilter(QObject *, QEvent *e);
};
