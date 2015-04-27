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
    QnImageButtonBar(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0, Qt::Orientation orientation = Qt::Horizontal);
    virtual ~QnImageButtonBar();

    void addButton(QnImageButtonWidget *button) {
        addButton(unusedMask(), button);
    }

    void addButton(int mask, QnImageButtonWidget *button);
    void removeButton(QnImageButtonWidget *button);
    QnImageButtonWidget *button(int mask) const;
    
    int visibleButtons() const;
    void setVisibleButtons(int buttonsVisibility);
    void setButtonsVisible(int mask, bool visible);

    int checkedButtons() const;
    void setCheckedButtons(int checkedButtons, bool silent = false);
    void setButtonsChecked(int mask, bool checked, bool silent = false);

    int enabledButtons() const;
    void setEnabledButtons(int enabledButtons);
    void setButtonsEnabled(int mask, bool enabled);

    const QSizeF &uniformButtonSize() const;
    void setUniformButtonSize(const QSizeF &uniformButtonSize);

    int unusedMask() const;

signals:
    void visibleButtonsChanged();
    void checkedButtonsChanged();
    void enabledButtonsChanged();

private slots:
    void at_button_visibleChanged();
    void at_button_toggled();
    void at_button_enabledChanged();

private:
    void submitEnabledButtons(int mask);
    void submitVisibleButtons();
    void submitCheckedButtons(int mask);
    void submitButtonSize(QnImageButtonWidget *button);

private:
    QMap<int, QnImageButtonWidget *> m_buttonByMask;
    QHash<QnImageButtonWidget *, int> m_maskByButton;
    int m_visibleButtons;
    int m_checkedButtons;
    int m_enabledButtons;
    QSizeF m_uniformButtonSize;
    QGraphicsLinearLayout *m_layout;

    bool m_updating, m_submitting;
};


#endif // QN_IMAGE_BUTTON_BAR_H
