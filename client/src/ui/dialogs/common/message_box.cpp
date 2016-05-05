#include "message_box.h"
#include "ui_message_box.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>

#include <ui/workaround/cancel_drag.h>

class QnMessageBoxPrivate : public QObject
{
    QnMessageBox *q_ptr;
    Q_DECLARE_PUBLIC(QnMessageBox)

public:
    QAbstractButton *clickedButton;
    QList<QAbstractButton *> customButtons;
    QAbstractButton *defaultButton;
    QAbstractButton *escapeButton;
    QnMessageBox::Icon icon;

    QnMessageBoxPrivate(QnMessageBox *parent);

    void init();
    void detectDefaultButton();
    void detectEscapeButton();
    void stylizeButtons();
    int execReturnCode(QAbstractButton *button) const;
};


QnMessageBoxPrivate::QnMessageBoxPrivate(QnMessageBox *parent)
    : QObject(parent),
      q_ptr(parent),
      clickedButton(nullptr),
      defaultButton(nullptr),
      escapeButton(nullptr),
      icon(QnMessageBox::NoIcon)
{
}

void QnMessageBoxPrivate::init()
{
    Q_Q(QnMessageBox);

    connect(q->ui->buttonBox, &QDialogButtonBox::clicked, this,
            [this](QAbstractButton *button) { clickedButton = button; }
    );

    q->ui->checkBox->hide();
    q->ui->informativeLabel->hide();

    QFont font = q->ui->mainLabel->font();
    font.setPixelSize(font.pixelSize() + 2);
    q->ui->mainLabel->setFont(font);
    q->ui->mainLabel->setForegroundRole(QPalette::Light);

    detectEscapeButton();
    detectDefaultButton();
}

void QnMessageBoxPrivate::detectDefaultButton()
{
    Q_Q(QnMessageBox);

    // Ok button automatically becomes default button
    defaultButton = q->ui->buttonBox->button(QDialogButtonBox::Ok);
    if (defaultButton)
        return;

    // if the message box has one AcceptRole button, make it the default button
    for (QAbstractButton *button: q->buttons())
    {
        if (q->buttonRole(button) == QDialogButtonBox::AcceptRole)
        {
            if (defaultButton)
            {
                // already detected!
                defaultButton = nullptr;
                break;
            }
            defaultButton = button;
        }
    }
}

void QnMessageBoxPrivate::detectEscapeButton()
{
    Q_Q(QnMessageBox);

    // Cancel button automatically becomes escape button
    escapeButton = q->ui->buttonBox->button(QDialogButtonBox::Cancel);
    if (escapeButton)
        return;

    // If there is only one button and it doesn't have AcceptRole, make it the escape button
    const QList<QAbstractButton *> buttons = q->buttons();
    if (buttons.size() == 1)
    {
        escapeButton = buttons.first();
        return;
    }

    // if the message box has one RejectRole button, make it the escape button
    for (QAbstractButton *button: q->buttons())
    {
        if (q->buttonRole(button) == QDialogButtonBox::RejectRole)
        {
            if (escapeButton)
            {
                // already detected!
                escapeButton = nullptr;
                break;
            }
            escapeButton = button;
        }
    }

    if (escapeButton)
        return;

    // if the message box has one NoRole button, make it the escape button
    for (int i = 0; i < buttons.size(); ++i)
    {
        if (q->buttonRole(buttons[i]) == QDialogButtonBox::NoRole)
        {
            if (escapeButton)
            {
                // already detected!
                escapeButton = nullptr;
                break;
            }
            escapeButton = buttons[i];
        }
    }
}

void QnMessageBoxPrivate::stylizeButtons()
{
    Q_Q(QnMessageBox);

    for (QAbstractButton *button: q->buttons())
        setAccentStyle(button, button == defaultButton);
}

int QnMessageBoxPrivate::execReturnCode(QAbstractButton *button) const
{
    Q_Q(const QnMessageBox);

    int ret = q->ui->buttonBox->standardButton(button);
    if (ret == QDialogButtonBox::NoButton)
        ret = customButtons.indexOf(button);
    if (ret == -1)
        return QDialogButtonBox::Cancel;

    return ret;
}


static QDialogButtonBox::StandardButton execMessageBox(
        QWidget *parent,
        QnMessageBox::Icon icon,
        int helpTopicId,
        const QString &title,
        const QString &text,
        QDialogButtonBox::StandardButtons buttons,
        QDialogButtonBox::StandardButton defaultButton)
{
    QnMessageBox msgBox(
                icon,
                helpTopicId,
                title,
                text,
                buttons,
                parent);

    msgBox.setDefaultButton(defaultButton);

    return static_cast<QDialogButtonBox::StandardButton>(msgBox.exec());
}


QnMessageBox::QnMessageBox(QWidget *parent)
    : base_type(parent),
      ui(new Ui_QnMessageBox),
      d_ptr(new QnMessageBoxPrivate(this))
{
    Q_D(QnMessageBox);

    ui->setupUi(this);
    d->init();
}

QnMessageBox::QnMessageBox(
        Icon icon,
        int helpTopicId,
        const QString &title,
        const QString &text,
        QDialogButtonBox::StandardButtons buttons,
        QWidget *parent,
        Qt::WindowFlags flags)
    : base_type(parent,
                helpTopicId == Qn::Empty_Help
                        ? flags : flags | Qt::WindowContextHelpButtonHint),
      ui(new Ui_QnMessageBox),
      d_ptr(new QnMessageBoxPrivate(this))
{
    Q_D(QnMessageBox);

    ui->setupUi(this);
    d->init();

    setHelpTopic(this, helpTopicId);
    setWindowTitle(title);
    setText(text);
    setStandardButtons(buttons);
    setIcon(icon);
}

QnMessageBox::~QnMessageBox()
{
}

void QnMessageBox::addButton(
        QAbstractButton *button,
        QDialogButtonBox::ButtonRole role)
{
    Q_D(QnMessageBox);

    ui->buttonBox->addButton(button, role);
    d->customButtons.append(button);

    if (!d->escapeButton)
        d->detectEscapeButton();
    if (!d->defaultButton)
        d->detectDefaultButton();
    d->stylizeButtons();
}

QPushButton *QnMessageBox::addButton(
        const QString &text,
        QDialogButtonBox::ButtonRole role)
{
    Q_D(QnMessageBox);

    QPushButton *addedButton = ui->buttonBox->addButton(text, role);

    if (!d->escapeButton)
        d->detectEscapeButton();
    if (!d->defaultButton)
        d->detectDefaultButton();
    d->stylizeButtons();

    return addedButton;
}

QPushButton *QnMessageBox::addButton(QDialogButtonBox::StandardButton button)
{
    Q_D(QnMessageBox);

    QPushButton *addedButton = ui->buttonBox->addButton(button);

    if (!d->escapeButton)
        d->detectEscapeButton();
    if (!d->defaultButton)
        d->detectDefaultButton();
    d->stylizeButtons();

    return addedButton;
}

void QnMessageBox::removeButton(QAbstractButton *button)
{
    Q_D(QnMessageBox);

    ui->buttonBox->removeButton(button);
    d->customButtons.removeOne(button);
    if (!ui->buttonBox->buttons().contains(d->escapeButton))
        d->detectEscapeButton();
    if (!ui->buttonBox->buttons().contains(d->defaultButton))
        d->detectDefaultButton();
    d->stylizeButtons();
}

QList<QAbstractButton *> QnMessageBox::buttons() const
{
    return ui->buttonBox->buttons();
}

QDialogButtonBox::ButtonRole QnMessageBox::buttonRole(
        QAbstractButton *button) const
{
    return ui->buttonBox->buttonRole(button);
}

void QnMessageBox::setStandardButtons(
        QDialogButtonBox::StandardButtons buttons)
{
    Q_D(QnMessageBox);

    ui->buttonBox->setStandardButtons(buttons);
    if (!ui->buttonBox->buttons().contains(d->escapeButton))
        d->detectEscapeButton();
    if (!ui->buttonBox->buttons().contains(d->defaultButton))
        d->detectDefaultButton();
    d->stylizeButtons();
}

QDialogButtonBox::StandardButtons QnMessageBox::standardButtons() const
{
    return ui->buttonBox->standardButtons();
}

QDialogButtonBox::StandardButton QnMessageBox::standardButton(
        QAbstractButton *button) const
{
    return ui->buttonBox->standardButton(button);
}

QPushButton *QnMessageBox::button(
        QDialogButtonBox::StandardButton which) const
{
    return ui->buttonBox->button(which);
}

QAbstractButton *QnMessageBox::defaultButton() const
{
    Q_D(const QnMessageBox);
    return d->defaultButton;
}

void QnMessageBox::setDefaultButton(QAbstractButton *button)
{
    if (!ui->buttonBox->buttons().contains(button))
        return;

    if (QPushButton *pushButton = qobject_cast<QPushButton*>(button))
        pushButton->setAutoDefault(true);
    button->setFocus();

    Q_D(QnMessageBox);

    d->stylizeButtons();
}

void QnMessageBox::setDefaultButton(QDialogButtonBox::StandardButton button)
{
    QPushButton *buttonWidget = ui->buttonBox->button(button);

    if (buttonWidget)
        setDefaultButton(buttonWidget);
}

QAbstractButton *QnMessageBox::escapeButton() const
{
    Q_D(const QnMessageBox);
    return d->escapeButton;
}

void QnMessageBox::setEscapeButton(QAbstractButton *button)
{
    Q_D(QnMessageBox);

    if (button && ui->buttonBox->buttons().contains(button))
        d->escapeButton = button;
    else
        d->escapeButton = nullptr;
}

void QnMessageBox::setEscapeButton(QDialogButtonBox::StandardButton button)
{
    setEscapeButton(ui->buttonBox->button(button));
}

QAbstractButton *QnMessageBox::clickedButton() const
{
    Q_D(const QnMessageBox);
    return d->clickedButton;
}

QString QnMessageBox::text() const
{
    return ui->mainLabel->text();
}

void QnMessageBox::setText(const QString &text)
{
    ui->mainLabel->setText(text);
}

QnMessageBox::Icon QnMessageBox::icon() const
{
    Q_D(const QnMessageBox);
    return d->icon;
}

void QnMessageBox::setIcon(QnMessageBox::Icon icon)
{
    Q_D(QnMessageBox);
    if (d->icon == icon)
        return;

    d->icon = icon;

    QString pixmapFile;

    switch (icon)
    {
    case Information:
        pixmapFile = lit("standard_icons/messagebox_information.png");
        break;
    case Warning:
        pixmapFile = lit("standard_icons/messagebox_warning.png");
        break;
    case Critical:
        pixmapFile = lit("standard_icons/messagebox_critical.png");
        break;
    case Question:
        pixmapFile = lit("standard_icons/messagebox_question.png");
        break;
    default:
        break;
    }

    QPixmap pixmap;
    if (!pixmapFile.isEmpty())
        pixmap = qnSkin->pixmap(pixmapFile);

    ui->iconLabel->setVisible(!pixmap.isNull());
    ui->iconLabel->setPixmap(pixmap);
    ui->iconLabel->resize(pixmap.size());
}

Qt::TextFormat QnMessageBox::textFormat() const
{
    return ui->mainLabel->textFormat();
}

void QnMessageBox::setTextFormat(Qt::TextFormat format)
{
    ui->mainLabel->setTextFormat(format);
}

QString QnMessageBox::informativeText() const
{
    return ui->informativeLabel->text();
}

void QnMessageBox::setInformativeText(const QString &text)
{
    ui->informativeLabel->setText(text);
    ui->informativeLabel->setVisible(!text.isEmpty());
}

QString QnMessageBox::checkBoxText() const
{
    return ui->checkBox->text();
}

void QnMessageBox::setCheckBoxText(const QString &text)
{
    ui->checkBox->setText(text);
    ui->checkBox->setVisible(!text.isEmpty());
}

bool QnMessageBox::isChecked() const
{
    return ui->checkBox->isChecked();
}

void QnMessageBox::setChecked(bool checked)
{
    ui->checkBox->setChecked(checked);
}

QDialogButtonBox::StandardButton QnMessageBox::information(
        QWidget *parent,
        int helpTopicId,
        const QString &title,
        const QString& text,
        QDialogButtonBox::StandardButtons buttons,
        QDialogButtonBox::StandardButton defaultButton)
{
    return execMessageBox(parent,
                          Information,
                          helpTopicId,
                          title,
                          text,
                          buttons,
                          defaultButton);
}

QDialogButtonBox::StandardButton QnMessageBox::information(
        QWidget *parent,
        const QString &title,
        const QString &text,
        QDialogButtonBox::StandardButtons buttons,
        QDialogButtonBox::StandardButton defaultButton)
{
    return execMessageBox(parent,
                          Information,
                          Qn::Empty_Help,
                          title,
                          text,
                          buttons,
                          defaultButton);
}

QDialogButtonBox::StandardButton QnMessageBox::question(
        QWidget *parent,
        int helpTopicId,
        const QString &title,
        const QString &text,
        QDialogButtonBox::StandardButtons buttons,
        QDialogButtonBox::StandardButton defaultButton)
{
    return execMessageBox(parent,
                          Question,
                          helpTopicId,
                          title,
                          text,
                          buttons,
                          defaultButton);
}

QDialogButtonBox::StandardButton QnMessageBox::question(
        QWidget *parent,
        const QString &title,
        const QString &text,
        QDialogButtonBox::StandardButtons buttons,
        QDialogButtonBox::StandardButton defaultButton)
{
    return execMessageBox(parent,
                          Question,
                          Qn::Empty_Help,
                          title,
                          text,
                          buttons,
                          defaultButton);
}

QDialogButtonBox::StandardButton QnMessageBox::warning(
        QWidget *parent,
        int helpTopicId,
        const QString &title,
        const QString &text,
        QDialogButtonBox::StandardButtons buttons,
        QDialogButtonBox::StandardButton defaultButton)
{
    return execMessageBox(parent,
                          Warning,
                          helpTopicId,
                          title,
                          text,
                          buttons,
                          defaultButton);
}

QDialogButtonBox::StandardButton QnMessageBox::warning(
        QWidget *parent,
        const QString &title,
        const QString &text,
        QDialogButtonBox::StandardButtons buttons,
        QDialogButtonBox::StandardButton defaultButton)
{
    return execMessageBox(parent,
                          Warning,
                          Qn::Empty_Help,
                          title,
                          text,
                          buttons,
                          defaultButton);
}

QDialogButtonBox::StandardButton QnMessageBox::critical(
        QWidget *parent,
        int helpTopicId,
        const QString &title,
        const QString &text,
        QDialogButtonBox::StandardButtons buttons,
        QDialogButtonBox::StandardButton defaultButton)
{
    return execMessageBox(parent,
                          Critical,
                          helpTopicId,
                          title,
                          text,
                          buttons,
                          defaultButton);
}

QDialogButtonBox::StandardButton QnMessageBox::critical(
        QWidget *parent,
        const QString &title,
        const QString &text,
        QDialogButtonBox::StandardButtons buttons,
        QDialogButtonBox::StandardButton defaultButton)
{
    return execMessageBox(parent,
                          Critical,
                          Qn::Empty_Help,
                          title,
                          text,
                          buttons,
                          defaultButton);
}

int QnMessageBox::exec()
{
    Q_D(QnMessageBox);

    Qt::WindowFlags flags = windowFlags();
    if (helpTopic(this) != Qn::Empty_Help)
        flags |= Qt::WindowContextHelpButtonHint;
    else
        flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags);

    /* We cannot cancel drag via modal dialog, let parent process it. */
    if (parentWidget())
        cancelDrag(parentWidget());

    adjustSize();

    base_type::exec();

    return d->execReturnCode(clickedButton());
}

void QnMessageBox::closeEvent(QCloseEvent *event)
{
    Q_D(QnMessageBox);

    if (!d->escapeButton)
    {
        event->ignore();
        return;
    }

    QDialog::closeEvent(event);

    d->clickedButton = d->escapeButton;
    setResult(d->execReturnCode(d->escapeButton));
}

void QnMessageBox::keyPressEvent(QKeyEvent *event)
{
    Q_D(QnMessageBox);

    if (event->key() == Qt::Key_Escape
#ifdef Q_OS_MAC
        || (event->modifiers() == Qt::ControlModifier
            && event->key() == Qt::Key_Period)
#endif
        )
    {
        if (d->escapeButton)
        {
#ifdef Q_OS_MAC
            d->escapeButton->animateClick();
#else
            d->escapeButton->click();
#endif
        }
        return;
    }

    if (event == QKeySequence::Copy)
    {
        QString separator = lit("---------------------------\n");
        QString textToCopy = separator;
        separator.prepend(QLatin1Char('\n'));
        textToCopy += windowTitle() + separator; // title
        textToCopy += ui->mainLabel->text() + separator; // text

        if (!ui->informativeLabel->text().isEmpty())
            textToCopy += ui->informativeLabel->text() + separator;

        QString buttonTexts;
        QList<QAbstractButton *> buttons = ui->buttonBox->buttons();
        for (int i = 0; i < buttons.count(); i++)
        {
            buttonTexts += buttons[i]->text() + QLatin1String("   ");
        }
        textToCopy += buttonTexts + separator;
        QApplication::clipboard()->setText(textToCopy);
        return;
    }

    if (!(event->modifiers() &
          (Qt::AltModifier | Qt::ControlModifier | Qt::MetaModifier)))
    {
        int key = event->key() & ~Qt::MODIFIER_MASK;
        if (key)
        {
            const QList<QAbstractButton *> buttons = ui->buttonBox->buttons();
            for (int i = 0; i < buttons.count(); ++i)
            {
                QAbstractButton *pb = buttons.at(i);
                QKeySequence shortcut = pb->shortcut();
                if (!shortcut.isEmpty() &&
                    key == int(shortcut[0] & ~Qt::MODIFIER_MASK))
                {
                    pb->animateClick();
                    return;
                }
            }
        }
    }

    QDialog::keyPressEvent(event);
}
