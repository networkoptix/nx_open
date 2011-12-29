#ifndef __QN_CHECKED_BUTTON_H__
#define __QN_CHECKED_BUTTON_H__

#include <QToolButton>

class QnCheckedButton: public QToolButton
{
public:
    QnCheckedButton(QWidget* parent);
    void setColor(const QColor& color);
    QColor color() const;
    void setCheckedColor(const QColor& color);
protected:
    void paintEvent(QPaintEvent * event);
    virtual void checkStateSet();
private:
    int m_checkonLastPaint;
    QColor m_color;
    QColor m_checkedColor;
    void fillButton();
};

#endif // __QN_CHECKED_BUTTON_H__
