#include "message_box.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>

#include <cassert>

#include <ui/help/help_topic_accessor.h>

static QMessageBox::StandardButton showNewMessageBox(QWidget *parent, QMessageBox::Icon icon, int helpTopicId, const QString& title, const QString& text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) {
    QnMessageBox msgBox(icon, helpTopicId, title, text, QMessageBox::NoButton, parent);
    QDialogButtonBox *buttonBox = msgBox.findChild<QDialogButtonBox *>();
    assert(buttonBox != 0);

    uint mask = QMessageBox::FirstButton;
    while (mask <= QMessageBox::LastButton) {
        uint sb = buttons & mask;
        mask <<= 1;
        if (!sb)
            continue;
        QPushButton *button = msgBox.addButton((QMessageBox::StandardButton)sb);
        // Choose the first accept role as the default
        if (msgBox.defaultButton())
            continue;
        if ((defaultButton == QMessageBox::NoButton && buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole)
            || (defaultButton != QMessageBox::NoButton && sb == uint(defaultButton)))
            msgBox.setDefaultButton(button);
    }
    if (msgBox.exec() == -1)
        return QMessageBox::Cancel;
    return msgBox.standardButton(msgBox.clickedButton());
}


QnMessageBox::QnMessageBox(QWidget *parent): 
    base_type(parent) 
{}

QnMessageBox::QnMessageBox(Icon icon, int helpTopicId, const QString &title, const QString &text, StandardButtons buttons, QWidget *parent, Qt::WindowFlags flags):
    base_type(icon, title, text, buttons, parent, helpTopicId == -1 ? flags : flags | Qt::WindowContextHelpButtonHint)
{
    setHelpTopic(this, helpTopicId);
}

QnMessageBox::StandardButton QnMessageBox::information(QWidget *parent, int helpTopicId, const QString &title, const QString& text, StandardButtons buttons, StandardButton defaultButton) {
    return showNewMessageBox(parent, Information, helpTopicId, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton QnMessageBox::question(QWidget *parent, int helpTopicId, const QString &title, const QString& text, StandardButtons buttons, StandardButton defaultButton) {
    return showNewMessageBox(parent, Question, helpTopicId, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton QnMessageBox::warning(QWidget *parent, int helpTopicId, const QString &title, const QString& text, StandardButtons buttons, StandardButton defaultButton) {
    return showNewMessageBox(parent, Warning, helpTopicId, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton QnMessageBox::critical(QWidget *parent, int helpTopicId, const QString &title, const QString& text, StandardButtons buttons, StandardButton defaultButton) {
    return showNewMessageBox(parent, Critical, helpTopicId, title, text, buttons, defaultButton);
}

QPushButton* QnMessageBox::addCustomButton(const QString &text, QMessageBox::ButtonRole role) {
    QPushButton* button = addButton(text, role);
    if (!button)
        return button;

    /* First-time setup */
    if (m_customButtons.isEmpty()) {
        QList<QDialogButtonBox *> buttonBoxes = findChildren<QDialogButtonBox *>();
        Q_ASSERT(!buttonBoxes.isEmpty());
        if (buttonBoxes.empty())
            return button;

        QDialogButtonBox* buttonBox = buttonBoxes.first();

        disconnect(buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(_q_buttonClicked(QAbstractButton*)));
        connect(this, SIGNAL(defaultButtonClicked(QAbstractButton*)), this, SLOT(_q_buttonClicked(QAbstractButton*)));

        connect(buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton* clicked) {
            if (m_customButtons.contains(clicked))
                return;
            emit defaultButtonClicked(clicked);
        }); 
    }
    
    m_customButtons << button;
    
    return button;
}
