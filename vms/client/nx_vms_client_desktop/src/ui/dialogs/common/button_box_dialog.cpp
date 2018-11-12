#include "button_box_dialog.h"

#include <QtCore/QEvent>

#include <QtWidgets/QPushButton>

#include <utils/common/warnings.h>
#include <ui/style/custom_style.h>

QnButtonBoxDialog::QnButtonBoxDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_clickedButton(QDialogButtonBox::NoButton),
    m_readOnly(false)
{
}

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

        if (QAbstractButton *okButton = m_buttonBox->button(QDialogButtonBox::Ok))
            setAccentStyle(okButton);
    }
}

bool QnButtonBoxDialog::event(QEvent *event) {
    bool result = base_type::event(event);

    if(event->type() == QEvent::Polish)
        initializeButtonBox();

    return result;
}

QDialogButtonBox* QnButtonBoxDialog::buttonBox() const {
    return m_buttonBox.data();
}

void QnButtonBoxDialog::initializeButtonBox() {
    if(m_buttonBox)
        return; /* Already initialized with a direct call to setButtonBox in derived class's constructor. */

    QList<QDialogButtonBox *> buttonBoxes = findChildren<QDialogButtonBox *>(QString(), Qt::FindDirectChildrenOnly);
    NX_ASSERT(buttonBoxes.size() == 1, Q_FUNC_INFO, "Invalid buttonBox count");

    if (buttonBoxes.isEmpty())
        buttonBoxes = findChildren<QDialogButtonBox *>(QString(), Qt::FindChildrenRecursively);

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
    Q_UNUSED(button);
}

bool QnButtonBoxDialog::isReadOnly() const
{
    return m_readOnly;
}

void QnButtonBoxDialog::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    m_readOnly = readOnly;
    setReadOnlyInternal();
}

void QnButtonBoxDialog::setReadOnlyInternal()
{
    if (!m_buttonBox || !m_readOnly)
        return;

    if (auto button = m_buttonBox->button(QDialogButtonBox::Ok))
        button->setFocus();
}
