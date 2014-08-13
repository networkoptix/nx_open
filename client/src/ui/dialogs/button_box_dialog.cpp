#include "button_box_dialog.h"

#include <QtCore/QEvent>

#include <utils/common/warnings.h>

QnButtonBoxDialog::QnButtonBoxDialog(QWidget *parent, Qt::WindowFlags windowFlags): 
    base_type(parent, windowFlags), 
    m_clickedButton(QDialogButtonBox::NoButton)
{}

QnButtonBoxDialog::~QnButtonBoxDialog() {
    return;
}

void QnButtonBoxDialog::setButtonBox(QDialogButtonBox *buttonBox) {
    if(m_buttonBox.data() == buttonBox)
        return;

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

bool QnButtonBoxDialog::event(QEvent *event) {
    bool result = base_type::event(event);

    if(event->type() == QEvent::Polish)
        initializeButtonBox();

    return result;
}

void QnButtonBoxDialog::initializeButtonBox() {
    if(m_buttonBox)
        return; /* Already initialized with a direct call to setButtonBox in derived class's constructor. */

    QList<QDialogButtonBox *> buttonBoxes = findChildren<QDialogButtonBox *>();
    if(buttonBoxes.empty()) {
        qnWarning("Button box dialog '%1' doesn't have a button box.", metaObject()->className());
        return;
    }
    if(buttonBoxes.size() > 1) {
        qnWarning("Button box dialog '%1' has several button boxes.", metaObject()->className());
        return;
    }

    setButtonBox(buttonBoxes[0]);
}

void QnButtonBoxDialog::at_buttonBox_clicked(QAbstractButton *button) {
    if(m_buttonBox)
        m_clickedButton = m_buttonBox.data()->standardButton(button);
    buttonBoxClicked(m_clickedButton);
}

void QnButtonBoxDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button) {
    //do nothing
}
