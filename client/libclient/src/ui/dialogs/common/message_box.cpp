#include "message_box.h"
#include "ui_message_box.h"

#include <QtGui/QStandardItemModel>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>

#include <ui/workaround/cancel_drag.h>

#include <utils/common/app_info.h>

class QnMessageBoxPrivate : public QObject
{
    QnMessageBox *q_ptr;
    Q_DECLARE_PUBLIC(QnMessageBox)

public:
    QAbstractButton *clickedButton;
    QList<QAbstractButton *> customButtons;
    QAbstractButton *defaultButton;
    QAbstractButton *escapeButton;
    QList<QLabel*> informativeLabels;
    QList<QWidget*> customWidgets;
    QnMessageBoxIcon icon;
    QPointer<QWidget> focusWidget;

    QnMessageBoxPrivate(QnMessageBox *parent);

    void init();
    void detectDefaultButton();
    void detectEscapeButton();
    void stylizeButtons();
    int execReturnCode(QAbstractButton *button) const;
};


QnMessageBoxPrivate::QnMessageBoxPrivate(QnMessageBox* parent) :
    QObject(parent),
    q_ptr(parent),
    clickedButton(nullptr),
    defaultButton(nullptr),
    escapeButton(nullptr),
    icon(QnMessageBoxIcon::NoIcon)
{
}

void QnMessageBoxPrivate::init()
{
    Q_Q(QnMessageBox);

    connect(q->ui->buttonBox, &QDialogButtonBox::clicked, this,
        [this](QAbstractButton *button) { clickedButton = button; });

    q->ui->checkBox->hide();
    q->ui->secondaryLine->hide();
    q->ui->iconLabel->hide();

    QFont font = q->ui->mainLabel->font();
    font.setPixelSize(font.pixelSize() + 2);
    q->ui->mainLabel->setFont(font);
    q->ui->mainLabel->setForegroundRole(QPalette::Light);

    q->setResizeToContentsMode(Qt::Vertical);

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

QnMessageBox::QnMessageBox(
    QWidget* parent
    , Qt::WindowFlags flags)
    :
    base_type(parent, flags),
    ui(new Ui::MessageBox),
    d_ptr(new QnMessageBoxPrivate(this))

{
    initialize();
}

QnMessageBox::QnMessageBox(
    QnMessageBoxIcon icon,
    const QString& text,
    const QString& extras,
    QDialogButtonBox::StandardButtons buttons,
    QDialogButtonBox::StandardButton defaultButton,
    QWidget* parent)
    :
    base_type(parent, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::MessageBox),
    d_ptr(new QnMessageBoxPrivate(this))
{
    initialize();

    if (!text.isEmpty())
        setText(text);
    setStandardButtons(buttons);
    setIcon(icon);
    if (!extras.isEmpty())
        setInformativeText(extras);

    setDefaultButton(defaultButton);
}

QnMessageBox::~QnMessageBox()
{
}

void QnMessageBox::initialize()
{
    Q_D(QnMessageBox);

    ui->setupUi(this);
    d->init();
    setWindowTitle(QnAppInfo::productNameLong());
}

QDialogButtonBox::StandardButton QnMessageBox::execute(
    QWidget* parent,
    QnMessageBoxIcon icon,
    const QString& text,
    const QString& extras,
    QDialogButtonBox::StandardButtons buttons,
    QDialogButtonBox::StandardButton defaultButton)
{
    QnMessageBox msgBox(icon, text, extras, buttons, defaultButton, parent);
    return static_cast<QDialogButtonBox::StandardButton>(msgBox.exec());
}

QDialogButtonBox::StandardButton QnMessageBox::information(
    QWidget* parent,
    const QString& text,
    const QString& extras,
    QDialogButtonBox::StandardButtons buttons,
    QDialogButtonBox::StandardButton defaultButton)
{
    return execute(parent, QnMessageBoxIcon::Information, text, extras, buttons, defaultButton);
}

QDialogButtonBox::StandardButton QnMessageBox::warning(
    QWidget* parent,
    const QString& text,
    const QString& extras,
    QDialogButtonBox::StandardButtons buttons,
    QDialogButtonBox::StandardButton defaultButton)
{
    return execute(parent, QnMessageBoxIcon::Warning, text, extras, buttons, defaultButton);
}

QDialogButtonBox::StandardButton QnMessageBox::question(
    QWidget* parent,
    const QString& text,
    const QString& extras,
    QDialogButtonBox::StandardButtons buttons,
    QDialogButtonBox::StandardButton defaultButton)
{
    return execute(parent, QnMessageBoxIcon::Question, text, extras, buttons, defaultButton);
}

QDialogButtonBox::StandardButton QnMessageBox::critical(
    QWidget* parent,
    const QString& text,
    const QString& extras,
    QDialogButtonBox::StandardButtons buttons,
    QDialogButtonBox::StandardButton defaultButton)
{
    return execute(parent, QnMessageBoxIcon::Critical, text, extras, buttons, defaultButton);
}

QDialogButtonBox::StandardButton QnMessageBox::success(
    QWidget* parent,
    const QString& text,
    const QString& extras,
    QDialogButtonBox::StandardButtons buttons,
    QDialogButtonBox::StandardButton defaultButton)
{
    return execute(parent, QnMessageBoxIcon::Success, text, extras, buttons, defaultButton);
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

QPushButton* QnMessageBox::addCustomButton(QnMessageBoxCustomButton button)
{
    switch (button)
    {
        case QnMessageBoxCustomButton::Overwrite:
            return addCustomButton(button, QDialogButtonBox::AcceptRole, QnButtonAccent::Warning);
        case QnMessageBoxCustomButton::Delete:
            return addCustomButton(button, QDialogButtonBox::AcceptRole, QnButtonAccent::Warning);
        case QnMessageBoxCustomButton::Reset:
            return addCustomButton(button, QDialogButtonBox::AcceptRole, QnButtonAccent::Warning);
        case QnMessageBoxCustomButton::Skip:
            return addCustomButton(button, QDialogButtonBox::RejectRole, QnButtonAccent::NoAccent);
        default:
            return nullptr;
    }
}

QPushButton* QnMessageBox::addCustomButton(
    QnMessageBoxCustomButton button,
    QDialogButtonBox::ButtonRole role,
    QnButtonAccent accent)
{
    QPushButton* result = nullptr;
    switch (button)
    {
        case QnMessageBoxCustomButton::Overwrite:
            return addButton(tr("Overwrite"), role, accent);
        case QnMessageBoxCustomButton::Delete:
            return addButton(tr("Delete"), role, accent);
        case QnMessageBoxCustomButton::Reset:
            return addButton(tr("Reset"), role, accent);
        case QnMessageBoxCustomButton::Skip:
            return addButton(tr("Skip"), role, accent);
        default:
            return nullptr;
    }
}

QPushButton* QnMessageBox::addButton(
    const QString &text,
    QDialogButtonBox::ButtonRole role,
    QnButtonAccent accent)
{
    Q_D(QnMessageBox);

    QPushButton* result = ui->buttonBox->addButton(text, role);
    d->customButtons.append(result);

    if (!d->escapeButton)
        d->detectEscapeButton();
    if (accent != QnButtonAccent::NoAccent)
        d->defaultButton = result;

    d->stylizeButtons();

    return result;
}

QPushButton *QnMessageBox::addButton(QDialogButtonBox::StandardButton button)
{
    Q_D(QnMessageBox);

    QPushButton *addedButton = ui->buttonBox->addButton(button);
    d->customButtons.append(addedButton);

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

void QnMessageBox::setDefaultButton(QAbstractButton* button)
{
    NX_ASSERT(ui->buttonBox->buttons().contains(button));
    if (!ui->buttonBox->buttons().contains(button))
        return;

    Q_D(QnMessageBox);
    if (button == d->defaultButton)
        return;

    if (QPushButton* pushButton = qobject_cast<QPushButton*>(button))
        pushButton->setDefault(true);

    button->setFocus();

    d->defaultButton = button;
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

QnMessageBoxIcon QnMessageBox::icon() const
{
    Q_D(const QnMessageBox);
    return d->icon;
}

QPixmap QnMessageBox::getPixmapByIconId(QnMessageBoxIcon icon)
{
    const auto standardPixmap =
        [this](QStyle::StandardPixmap pixmapId) -> QPixmap
        {
            return QnSkin::maximumSizePixmap(style()->standardIcon(pixmapId));
        };

    switch (icon)
    {
        case QnMessageBoxIcon::Information:
            return standardPixmap(QStyle::SP_MessageBoxInformation);
        case QnMessageBoxIcon::Warning:
            return standardPixmap(QStyle::SP_MessageBoxWarning);
        case QnMessageBoxIcon::Critical:
            return standardPixmap(QStyle::SP_MessageBoxCritical);
        case QnMessageBoxIcon::Question:
            return standardPixmap(QStyle::SP_MessageBoxQuestion);
        case QnMessageBoxIcon::Success:
            return qnSkin->pixmap("standard_icons/message_box_success.png",
                QSize(), Qt::IgnoreAspectRatio, Qt::FastTransformation, true);
        default:
            return QPixmap();
    }
}

void QnMessageBox::setIcon(QnMessageBoxIcon icon)
{
    Q_D(QnMessageBox);
    if (d->icon == icon)
        return;

    d->icon = icon;

    const auto pixmap = getPixmapByIconId(icon);
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
    Q_D(const QnMessageBox);
    QStringList lines;
    for (QLabel* label: d->informativeLabels)
        lines << label->text();

    return lines.join(L'\n');
}

void QnMessageBox::setInformativeText(const QString &text, bool split)
{
    Q_D(QnMessageBox);
    QStringList lines = split
        ? text.split(L'\n', QString::SkipEmptyParts)
        : QStringList() << text;

    for (QLabel* label: d->informativeLabels)
        delete label;
    d->informativeLabels.clear();

    int index = ui->verticalLayout->indexOf(ui->mainLabel) + 1;
    for (const QString& line: lines)
    {
        QLabel* label = new QLabel(this);
        label->setWordWrap(true);
        label->setText(line);
        ui->verticalLayout->insertWidget(index, label);
        d->informativeLabels.append(label);
        ++index;
    }
}

void QnMessageBox::addCustomWidget(QWidget* widget, Layout layout, int stretch,
    Qt::Alignment alignment, bool focusThisWidget)
{
    Q_D(QnMessageBox);
    NX_ASSERT(!d->customWidgets.contains(widget));
    d->customWidgets << widget;

    widget->setParent(this);

    switch (layout)
    {
        case QnMessageBox::Layout::Main:
            ui->mainLayout->insertWidget(
                ui->mainLayout->indexOf(ui->line),
                widget,
                stretch,
                alignment
            );
            ui->secondaryLine->setVisible(true);
            break;
        case QnMessageBox::Layout::Content:
            ui->verticalLayout->insertWidget(
                ui->verticalLayout->indexOf(ui->checkBox),
                widget,
                stretch,
                alignment
            );
        default:
            break;
    }

    if (focusThisWidget)
    {
        d->focusWidget = widget;
        if (isVisible())
            widget->setFocus();
    }
}

void QnMessageBox::removeCustomWidget(QWidget* widget)
{
    Q_D(QnMessageBox);
    NX_ASSERT(d->customWidgets.contains(widget));
    d->customWidgets.removeOne(widget);

    bool showSecondaryLine = std::any_of(
        d->customWidgets.cbegin(), d->customWidgets.cend(),
        [this](QWidget* w)
        {
            return ui->mainLayout->indexOf(w) >= 0;
        });
    ui->secondaryLine->setVisible(showSecondaryLine);
    ui->mainLayout->removeWidget(widget);
    ui->verticalLayout->removeWidget(widget);

    if (d->focusWidget == widget)
        d->focusWidget = nullptr;
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

int QnMessageBox::exec()
{
    Q_D(QnMessageBox);

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

    base_type::closeEvent(event);

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

        auto info = informativeText();
        if (!info.isEmpty())
            textToCopy += info + separator;

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

    base_type::keyPressEvent(event);
}

void QnMessageBox::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);

    Q_D(QnMessageBox);
    if (d->focusWidget)
        d->focusWidget->setFocus();
}

void QnMessageBox::afterLayout()
{
    if (hasHeightForWidth())
    {
        setFixedHeight(heightForWidth(width()));
    }
    else
    {
        /* This dialog must have height-for-width, but just in case handle otherwise: */
        setFixedHeight(sizeHint().height());
    }
}
