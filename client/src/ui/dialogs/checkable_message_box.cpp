#include "checkable_message_box.h"

#include <QtCore/QDebug>
#include <QtCore/QVariant>

#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>

#include <ui/help/help_topic_accessor.h>

class QnCheckableMessageBoxPrivate {
public:
    QnCheckableMessageBoxPrivate(QDialog *q): 
        clickedButton(0)     
    {
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

        pixmapLabel = new QLabel(q);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(pixmapLabel->sizePolicy().hasHeightForWidth());
        pixmapLabel->setSizePolicy(sizePolicy);
        pixmapLabel->setVisible(false);

        QSpacerItem *pixmapSpacer = new QSpacerItem(0, 5, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);

        messageLabel = new QLabel(q);
        messageLabel->setMinimumSize(QSize(300, 0));
        messageLabel->setWordWrap(true);
        messageLabel->setOpenExternalLinks(true);
        messageLabel->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard|Qt::LinksAccessibleByMouse);

        QSpacerItem *checkBoxRightSpacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
        QSpacerItem *buttonSpacer = new QSpacerItem(0, 1, QSizePolicy::Minimum, QSizePolicy::Minimum);

        checkBox = new QCheckBox(q);
        checkBox->setText(QnCheckableMessageBox::tr("Do not ask again"));

        buttonBox = new QDialogButtonBox(q);
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        QVBoxLayout *verticalLayout = new QVBoxLayout();
        verticalLayout->addWidget(pixmapLabel);
        verticalLayout->addItem(pixmapSpacer);

        QHBoxLayout *horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->addLayout(verticalLayout);
        horizontalLayout_2->addWidget(messageLabel);

        QHBoxLayout *horizontalLayout = new QHBoxLayout();
        horizontalLayout->addWidget(checkBox);
        horizontalLayout->addItem(checkBoxRightSpacer);

        QVBoxLayout *verticalLayout_2 = new QVBoxLayout(q);
        verticalLayout_2->addLayout(horizontalLayout_2);
        verticalLayout_2->addLayout(horizontalLayout);
        verticalLayout_2->addItem(buttonSpacer);
        verticalLayout_2->addWidget(buttonBox);
    }

    QLabel *pixmapLabel;
    QLabel *messageLabel;
    QCheckBox *checkBox;
    QDialogButtonBox *buttonBox;
    QAbstractButton *clickedButton;
};

QnCheckableMessageBox::QnCheckableMessageBox(QWidget *parent):
    QDialog(parent),
    d(new QnCheckableMessageBoxPrivate(this))
{
    setModal(true);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    connect(d->buttonBox,   SIGNAL(accepted()),                 this,   SLOT(accept()));
    connect(d->buttonBox,   SIGNAL(rejected()),                 this,   SLOT(reject()));
    connect(d->buttonBox,   SIGNAL(clicked(QAbstractButton*)),  this,   SLOT(at_buttonBox_clicked(QAbstractButton*)));
}

QnCheckableMessageBox::~QnCheckableMessageBox() {
    return;
}

void QnCheckableMessageBox::at_buttonBox_clicked(QAbstractButton *b) {
    d->clickedButton = b;
}

QAbstractButton *QnCheckableMessageBox::clickedButton() const {
    return d->clickedButton;
}

QDialogButtonBox::StandardButton QnCheckableMessageBox::clickedStandardButton(QDialogButtonBox::StandardButton defaultButton) const {
    if (d->clickedButton)
        return d->buttonBox->standardButton(d->clickedButton);
    return defaultButton;
}

QString QnCheckableMessageBox::text() const {
    return d->messageLabel->text();
}

void QnCheckableMessageBox::setText(const QString &t) {
    d->messageLabel->setTextFormat(Qt::AutoText);
    d->messageLabel->setText(t);
}

void QnCheckableMessageBox::setRichText(const QString &text) {
    d->messageLabel->setTextFormat(Qt::RichText);
    d->messageLabel->setText(text);
}

QPixmap QnCheckableMessageBox::iconPixmap() const {
    if (const QPixmap *p = d->pixmapLabel->pixmap())
        return QPixmap(*p);
    return QPixmap();
}

void QnCheckableMessageBox::setIconPixmap(const QPixmap &p) {
    d->pixmapLabel->setPixmap(p);
    d->pixmapLabel->setVisible(!p.isNull());
}

bool QnCheckableMessageBox::isChecked() const {
    return d->checkBox->isChecked();
}

void QnCheckableMessageBox::setChecked(bool s) {
    d->checkBox->setChecked(s);
}

QString QnCheckableMessageBox::checkBoxText() const {
    return d->checkBox->text();
}

void QnCheckableMessageBox::setCheckBoxText(const QString &t) {
    d->checkBox->setText(t);
}

bool QnCheckableMessageBox::isCheckBoxVisible() const {
    return d->checkBox->isVisible();
}

void QnCheckableMessageBox::setCheckBoxVisible(bool v) {
    d->checkBox->setVisible(v);
}

QDialogButtonBox::StandardButtons QnCheckableMessageBox::standardButtons() const {
    return d->buttonBox->standardButtons();
}

void QnCheckableMessageBox::setStandardButtons(QDialogButtonBox::StandardButtons s) {
    d->buttonBox->setStandardButtons(s);
}

QPushButton *QnCheckableMessageBox::button(QDialogButtonBox::StandardButton b) const {
    return d->buttonBox->button(b);
}

QPushButton *QnCheckableMessageBox::addButton(const QString &text, QDialogButtonBox::ButtonRole role) {
    return d->buttonBox->addButton(text, role);
}

QDialogButtonBox::StandardButton QnCheckableMessageBox::defaultButton() const {
    foreach (QAbstractButton *b, d->buttonBox->buttons())
        if (QPushButton *pb = qobject_cast<QPushButton *>(b))
            if (pb->isDefault())
               return d->buttonBox->standardButton(pb);
    return QDialogButtonBox::NoButton;
}

void QnCheckableMessageBox::setDefaultButton(QDialogButtonBox::StandardButton s) {
    if (QPushButton *b = d->buttonBox->button(s)) {
        b->setDefault(true);
        b->setFocus();
    }
}

QDialogButtonBox::StandardButton
QnCheckableMessageBox::question(QWidget *parent, int helpTopicId, const QString &title, const QString &question, const QString &checkBoxText, bool *checkBoxSetting,
        QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton, QDialogButtonBox::StandardButton cancelButton) {
    QnCheckableMessageBox mb(parent);
    mb.setWindowTitle(title);
    mb.setIconPixmap(QMessageBox::standardIcon(QMessageBox::Question));
    mb.setText(question);
    if (!checkBoxText.isEmpty())
        mb.setCheckBoxText(checkBoxText);
    mb.setChecked(*checkBoxSetting);
    mb.setStandardButtons(buttons);
    mb.setDefaultButton(defaultButton);
    setHelpTopic(&mb, helpTopicId);
    mb.exec();
    *checkBoxSetting = mb.isChecked();
    return mb.clickedStandardButton(cancelButton);
}

QDialogButtonBox::StandardButton
QnCheckableMessageBox::question(QWidget *parent, const QString &title, const QString &question, const QString &checkBoxText, bool *checkBoxSetting, 
        QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton, QDialogButtonBox::StandardButton cancelButton) {
    return QnCheckableMessageBox::question(parent, -1, title, question, checkBoxText, checkBoxSetting, buttons, defaultButton, cancelButton);
}

QDialogButtonBox::StandardButton
QnCheckableMessageBox::warning(QWidget *parent, int helpTopicId, const QString &title, const QString &warning, const QString &checkBoxText, bool *checkBoxSetting, 
        QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton, QDialogButtonBox::StandardButton cancelButton) {
    QnCheckableMessageBox mb(parent);
    mb.setWindowTitle(title);
    mb.setIconPixmap(QMessageBox::standardIcon(QMessageBox::Warning));
    mb.setText(warning);
    if (!checkBoxText.isEmpty())
        mb.setCheckBoxText(checkBoxText);
    mb.setChecked(*checkBoxSetting);
    mb.setStandardButtons(buttons);
    mb.setDefaultButton(defaultButton);
    setHelpTopic(&mb, helpTopicId);
    mb.exec();
    *checkBoxSetting = mb.isChecked();
    return mb.clickedStandardButton(cancelButton);
}

QDialogButtonBox::StandardButton
QnCheckableMessageBox::warning(QWidget *parent, const QString &title, const QString &warning, const QString &checkBoxText, bool *checkBoxSetting, 
        QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton, QDialogButtonBox::StandardButton cancelButton) {
    return QnCheckableMessageBox::warning(parent, -1, title, warning, checkBoxText, checkBoxSetting, buttons, defaultButton, cancelButton);
}
