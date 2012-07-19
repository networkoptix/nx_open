#ifndef QN_IMAGE_BUTTON_BAR_H
#define QN_IMAGE_BUTTON_BAR_H

#include <QtCore/QHash>
#include <QtCore/QMap>

#include <ui/graphics/items/standard/graphics_widget.h>

class QGraphicsLinearLayout;

class QnImageButtonWidget;

class QnImageButtonBar: public GraphicsWidget {
    Q_OBJECT;

    typedef GraphicsWidget base_type;

public:
    QnImageButtonBar(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnImageButtonBar();

    void addButton(int mask, QnImageButtonWidget *button);
    void removeButton(QnImageButtonWidget *button);
    QnImageButtonWidget *button(int mask) const;
    
    int buttonsVisibility() const;
    void setButtonsVisibility(int buttonsVisibility);
    void setButtonVisible(int mask, bool visible);

    const QSizeF &uniformButtonSize() const;
    void setUniformButtonSize(const QSizeF &uniformButtonSize);

signals:
    void buttonsVisibilityChanged();

private:
    void updateButtons();
    void updateButtonSize(QnImageButtonWidget *button);

private:
    QMap<int, QnImageButtonWidget *> m_buttonByMask;
    QHash<QnImageButtonWidget *, int> m_maskByButton;
    int m_buttonsVisibility;

    QSizeF m_uniformButtonSize;

    QGraphicsLinearLayout *m_layout;
};


#endif // QN_IMAGE_BUTTON_BAR_H
