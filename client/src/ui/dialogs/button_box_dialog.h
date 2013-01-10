#ifndef QN_BUTTON_BOX_DIALOG_H
#define QN_BUTTON_BOX_DIALOG_H

#include <QtCore/QWeakPointer>

#include <QtGui/QDialog>
#include <QtGui/QDialogButtonBox>

#include "utils/common/adl_connective.h"

/**
 * Button box dialog that can be queried for the button that was clicked to close it.
 */
class QnButtonBoxDialog: public AdlConnective<QDialog> {
    Q_OBJECT;

    typedef AdlConnective<QDialog> base_type;

public:
    QnButtonBoxDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0): 
        base_type(parent, windowFlags), 
        m_clickedButton(QDialogButtonBox::NoButton)
    {}

    QDialogButtonBox::StandardButton clickedButton() const {
        return m_clickedButton;
    }
    
protected:
    void setButtonBox(QDialogButtonBox *buttonBox) {
        if(m_buttonBox) {
            disconnect(m_buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(at_buttonBox_clicked(QAbstractButton *)));
            disconnect(m_buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
            disconnect(m_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
        }

        m_buttonBox = buttonBox;

        if(m_buttonBox) {
            connect(m_buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(at_buttonBox_clicked(QAbstractButton *)));
            connect(m_buttonBox, SIGNAL(accepted()),                 this, SLOT(accept()));
            connect(m_buttonBox, SIGNAL(rejected()),                 this, SLOT(reject()));
        }
    }

private slots:
    void at_buttonBox_clicked(QAbstractButton *button) {
        if(m_buttonBox)
            m_clickedButton = m_buttonBox.data()->standardButton(button);
    }

private:
    QDialogButtonBox::StandardButton m_clickedButton;
    QWeakPointer<QDialogButtonBox> m_buttonBox;
};

#endif // QN_BUTTON_BOX_DIALOG_H
