#ifndef QN_CHECKED_BUTTON_H
#define QN_CHECKED_BUTTON_H

#include <QtWidgets/QToolButton>

class QnCheckedButton: public QToolButton {
    Q_OBJECT

    typedef QToolButton base_type;

public:
    QnCheckedButton(QWidget* parent);
    
    QColor color() const;
    void setColor(const QColor& color);

    void setInsideColor(const QColor& color);

protected:
    virtual bool event(QEvent *event) override;

    void updateIcon();
    QPixmap generatePixmap(int size, const QColor &color, const QColor &insideColor, bool hovered, bool checked, bool enabled);

private:
    QColor m_color;
    QColor m_insideColor;
};

#endif // QN_CHECKED_BUTTON_H
