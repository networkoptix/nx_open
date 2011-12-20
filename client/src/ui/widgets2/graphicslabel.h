#ifndef QN_GRAPHICS_LABEL_H
#define QN_GRAPHICS_LABEL_H

#if 0
#include "graphicsframe.h"

class GraphicsLabelPrivate;

class GraphicsLabel : public GraphicsFrame
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Qt::TextFormat textFormat READ textFormat WRITE setTextFormat)
    Q_PROPERTY(QPixmap pixmap READ pixmap WRITE setPixmap)
    Q_PROPERTY(bool scaledContents READ hasScaledContents WRITE setScaledContents)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(bool wordWrap READ wordWrap WRITE setWordWrap)
    Q_PROPERTY(int margin READ margin WRITE setMargin)
    Q_PROPERTY(int indent READ indent WRITE setIndent)

public:
    explicit GraphicsLabel(QGraphicsItem *parent = 0, Qt::WindowFlags f = 0);
    explicit GraphicsLabel(const QString &text, QGraphicsItem *parent = 0, Qt::WindowFlags f = 0);
    ~GraphicsLabel();

    QString text() const;
    const QPixmap *pixmap() const;

    Qt::TextFormat textFormat() const;
    void setTextFormat(Qt::TextFormat);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment);

    void setWordWrap(bool on);
    bool wordWrap() const;

    int indent() const;
    void setIndent(int);

    int margin() const;
    void setMargin(int);

    bool hasScaledContents() const;
    void setScaledContents(bool);
    
public Q_SLOTS:
    void setText(const QString &);
    void setPixmap(const QPixmap &);
    void setNum(int);
    void setNum(double);
    void clear();

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    virtual bool event(QEvent *e);
    virtual void keyPressEvent(QKeyEvent *ev);
    virtual void paintEvent(QPaintEvent *);
    virtual void changeEvent(QEvent *);
    virtual void mousePressEvent(QMouseEvent *ev);
    virtual void mouseMoveEvent(QMouseEvent *ev);
    virtual void mouseReleaseEvent(QMouseEvent *ev);
    virtual void contextMenuEvent(QContextMenuEvent *ev);
    virtual void focusInEvent(QFocusEvent *ev);
    virtual void focusOutEvent(QFocusEvent *ev);
    virtual bool focusNextPrevChild(bool next);

private:
    Q_DISABLE_COPY(GraphicsLabel)
    Q_DECLARE_PRIVATE(GraphicsLabel)
};

#endif

#endif // QN_GRAPHICS_LABEL_H
