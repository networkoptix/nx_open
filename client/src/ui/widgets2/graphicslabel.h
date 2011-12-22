#ifndef QN_GRAPHICS_LABEL_H
#define QN_GRAPHICS_LABEL_H

#include "graphicsframe.h"

class GraphicsLabelPrivate;

class GraphicsLabel : public GraphicsFrame
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

    typedef GraphicsFrame base_type;

public:
    explicit GraphicsLabel(QGraphicsItem *parent = 0, Qt::WindowFlags f = 0);
    explicit GraphicsLabel(const QString &text, QGraphicsItem *parent = 0, Qt::WindowFlags f = 0);
    ~GraphicsLabel();

    QString text() const;
    
public Q_SLOTS:
    void setText(const QString &);
    void setNum(int);
    void setNum(double);
    void clear();

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;

    virtual void changeEvent(QEvent *event) override;

private:
    Q_DISABLE_COPY(GraphicsLabel)
    Q_DECLARE_PRIVATE(GraphicsLabel)
};


#endif // QN_GRAPHICS_LABEL_H
