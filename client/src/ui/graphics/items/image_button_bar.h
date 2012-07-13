#ifndef QN_IMAGE_BUTTON_BAR_H
#define QN_IMAGE_BUTTON_BAR_H

#include <ui/graphics/items/standard/graphics_widget.h>

class QnImageButtonWidget;

class QnImageButtonBar: public GraphicsWidget {
    Q_OBJECT;

    typedef GraphicsWidget base_type;

public:
    QnImageButtonBar(QGraphicsItem *parent, Qt::WindowFlags windowFlags = 0);
    virtual ~QnImageButtonBar();

    void addButton(int mask, QnImageButtonWidget *button);
    void removeButton(QnImageButtonWidget *button);
    QnImageButtonWidget *button(int mask) const;
    
    int buttonsVisibility() const;
    void setButtonsVisibility(int mask);
    void setButtonVisible(int mask, bool visible);

signals:
    void buttonsVisibilityChanged();

private:
    QHash<int, QnImageButtonWidget *> m_buttonByMask;
    QHash<QnImageButtonWidget *, int> m_maskByButton;
    int m_buttonsVisibility;

    QGraphicsLinearLayout *m_layout;
};


#endif // QN_IMAGE_BUTTON_BAR_H
