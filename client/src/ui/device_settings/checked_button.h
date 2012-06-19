#ifndef __QN_CHECKED_BUTTON_H__
#define __QN_CHECKED_BUTTON_H__

#include <QToolButton>

class QnCheckedButton: public QToolButton
{
public:
    QnCheckedButton(QWidget* parent);
    
    QColor color() const;
    void setColor(const QColor& color);

    void setInsideColor(const QColor& color);

    QColor checkedColor() const;
    void setCheckedColor(const QColor& color);

    void setEnabled(bool value);
protected:
    void updateIcon();
    QPixmap generatePixmap(int size, const QColor &color, const QColor &insideColor, bool hovered, bool checked);
private:
    QColor m_color;
    QColor m_insideColor;
    QColor m_checkedColor;
    bool m_insideColorDefined;
    bool m_isEnabled;
};

#endif // __QN_CHECKED_BUTTON_H__
