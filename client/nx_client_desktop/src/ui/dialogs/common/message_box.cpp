#include "message_box.h"
#include "ui_message_box.h"

#include <QtGui/QClipboard>
#include <QtGui/QCloseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QStandardItemModel>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>

#include <ui/workaround/cancel_drag.h>

namespace {

static constexpr int kMinimumWidth = 400;

} // namespace

class QnMessageBoxPrivate : public QObject
{
    QnMessageBox* q_ptr;
    Q_DECLARE_PUBLIC(QnMessageBox)

public:
    QAbstractButton* clickedButton;
    QList<QAbstractButton*> customButtons;
    QAbstractButton* defaultButton;
    Qn::ButtonAccent buttonAccent;
    QAbstractButton* escapeButton;
    QList<QLabel*> informativeLabels;
    QList<QWidget*> customWidgets;
    QnMessageBoxIcon icon;
    QPointer<QWidget> focusWidget;
    QnButtonDetections buttonDetection;
    Qt::TextFormat informativeTextFormat = Qt::AutoText;

public:
    QnMessageBoxPrivate(QnMessageBox* parent);

    void init();
    void detectDefaultButton();
    void detectEscapeButton();
    void stylizeButtons();
    int execReturnCode(QAbstractButton* button) const;

private:
    void detectSpecificButton(
        QAbstractButton*& refButton,
        QDialogButtonBox::StandardButton standardButton,
        QVector<QDialogButtonBox::ButtonRole> acceptedRoles);
};


QnMessageBoxPrivate::QnMessageBoxPrivate(QnMessageBox* parent) :
    QObject(parent),
    q_ptr(parent),
    clickedButton(nullptr),
    defaultButton(nullptr),
    buttonAccent(Qn::ButtonAccent::Standard),
    escapeButton(nullptr),
    icon(QnMessageBoxIcon::NoIcon),
    buttonDetection(QnButtonDetection::DefaultButton)
{
}

void QnMessageBoxPrivate::init()
{
    Q_Q(QnMessageBox);

    connect(q->ui->buttonBox, &QDialogButtonBox::clicked, this,
        [this](QAbstractButton* button)
        {
            Q_Q(QnMessageBox);
            clickedButton = button;
            const auto role = q->ui->buttonBox->buttonRole(button);
            switch (role)
            {
                case QDialogButtonBox::AcceptRole:
                case QDialogButtonBox::YesRole:
                case QDialogButtonBox::RejectRole:
                case QDialogButtonBox::NoRole:
                case QDialogButtonBox::HelpRole:
                    // These roles are handled in QDialogButtonBox
                    break;
                default:
                    q->accept();
                    break;
            }
        });

    q->ui->checkBoxWidget->hide();
    q->ui->secondaryLine->hide();
    q->ui->iconLabel->hide();

    QFont font = q->ui->mainLabel->font();
    font.setPixelSize(font.pixelSize() + 2);
    q->ui->mainLabel->setFont(font);
    q->ui->mainLabel->setForegroundRole(QPalette::Light);
    q->ui->mainLabel->setOpenExternalLinks(true);
    q->setResizeToContentsMode(Qt::Vertical);
}

void QnMessageBoxPrivate::detectSpecificButton(
    QAbstractButton*& refSpecificButton,
    QDialogButtonBox::StandardButton standardButton,
    QVector<QDialogButtonBox::ButtonRole> validRoles)
{
    Q_Q(QnMessageBox);

    // Don't look for button if it is specified
    if (refSpecificButton)
        return;

    const auto buttons = q->ui->buttonBox->buttons();
    if (buttons.size() == 1)
    {
        // Single button automatically becomes "specific"
        refSpecificButton = buttons.first();
        return;
    }

    // Ok button automatically becomes default button
    refSpecificButton = q->ui->buttonBox->button(standardButton);
    if (refSpecificButton)
        return;

    QMultiHash<QDialogButtonBox::ButtonRole, QAbstractButton*> buttonsByRole;

    for (QAbstractButton* button: q->buttons())
        buttonsByRole.insert(q->buttonRole(button), button);

    for (const auto role: validRoles)
    {
        const auto buttons = buttonsByRole.values(role);
        if (buttons.size() == 1)
        {
            refSpecificButton = buttons.first();
            break;
        }
    }
}

void QnMessageBoxPrivate::detectDefaultButton()
{
    detectSpecificButton(defaultButton, QDialogButtonBox::Ok,
        { QDialogButtonBox::AcceptRole, QDialogButtonBox::YesRole, QDialogButtonBox::ApplyRole });
}

void QnMessageBoxPrivate::detectEscapeButton()
{
    detectSpecificButton(escapeButton, QDialogButtonBox::Cancel,
        { QDialogButtonBox::RejectRole, QDialogButtonBox::NoRole });
}

void QnMessageBoxPrivate::stylizeButtons()
{
    Q_Q(QnMessageBox);

    for (QAbstractButton *button : q->buttons())
    {
        if (button != defaultButton) //< Only single default button is allowed
        {
            resetButtonStyle(button);
            continue;
        }

        if (buttonAccent == Qn::ButtonAccent::Warning)
            setWarningButtonStyle(button);
        else
            setAccentStyle(button);
    }
}

int QnMessageBoxPrivate::execReturnCode(QAbstractButton* button) const
{
    Q_Q(const QnMessageBox);

    int ret = q->ui->buttonBox->standardButton(button);
    if (ret == QDialogButtonBox::NoButton)
        ret = customButtons.indexOf(button);
    if (ret == -1)
        return QDialogButtonBox::Cancel;

    return ret;
}

QnMessageBox::QnMessageBox(QWidget* parent):
    base_type(parent, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::MessageBox),
    d_ptr(new QnMessageBoxPrivate(this))
{
    initialize();
    setStandardButtons(QDialogButtonBox::NoButton);
}

QnMessageBox::QnMessageBox(
    QnMessageBoxIcon icon,
    const QString& text,
    const QString& extras,
    QDialogButtonBox::StandardButtons buttons,
    QDialogButtonBox::StandardButton defaultButton,
    QWidget* parent)
    :
    QnMessageBox(parent)
{
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

void QnMessageBox::addButton(QAbstractButton* button, QDialogButtonBox::ButtonRole role)
{
    Q_D(QnMessageBox);

    ui->buttonBox->addButton(button, role);
    d->customButtons.append(button);
}

QPushButton* QnMessageBox::addCustomButton(
    QnMessageBoxCustomButton button,
    QDialogButtonBox::ButtonRole role,
    Qn::ButtonAccent accent)
{
    switch (button)
    {
        case QnMessageBoxCustomButton::Overwrite:
            return addButton(tr("Overwrite"), role, accent);
        case QnMessageBoxCustomButton::Delete:
            return addButton(tr("Delete"), role, accent);
        case QnMessageBoxCustomButton::Reset:
            return addButton(tr("Reset"), role, accent);
        case QnMessageBoxCustomButton::Close:
            return addButton(tr("Close"), role, accent);
        case QnMessageBoxCustomButton::Skip:
            return addButton(tr("Skip"), role, accent);
        default:
            return nullptr;
    }
}

QPushButton* QnMessageBox::addButton(
    const QString& text,
    QDialogButtonBox::ButtonRole role,
    Qn::ButtonAccent accent)
{
    Q_D(QnMessageBox);

    QPushButton* result = ui->buttonBox->addButton(text, role);
    d->customButtons.append(result);

    if (accent != Qn::ButtonAccent::NoAccent)
    {
        d->buttonAccent = accent;
        NX_ASSERT(!d->defaultButton, "Default button should not be set by now");
        d->defaultButton = result;
        d->buttonDetection &= ~int(QnButtonDetection::DefaultButton);
        d->stylizeButtons();
    }

    return result;
}

QPushButton* QnMessageBox::addButton(QDialogButtonBox::StandardButton button)
{
    Q_D(QnMessageBox);

    QPushButton* addedButton = ui->buttonBox->addButton(button);
    d->customButtons.append(addedButton);

    return addedButton;
}

void QnMessageBox::removeButton(QAbstractButton* button)
{
    Q_D(QnMessageBox);

    ui->buttonBox->removeButton(button);
    d->customButtons.removeOne(button);
}

QList<QAbstractButton*> QnMessageBox::buttons() const
{
    return ui->buttonBox->buttons();
}

QDialogButtonBox::ButtonRole QnMessageBox::buttonRole(QAbstractButton* button) const
{
    return ui->buttonBox->buttonRole(button);
}

void QnMessageBox::setStandardButtons(QDialogButtonBox::StandardButtons buttons)
{
    ui->buttonBox->setStandardButtons(buttons);
}

QDialogButtonBox::StandardButtons QnMessageBox::standardButtons() const
{
    return ui->buttonBox->standardButtons();
}

QDialogButtonBox::StandardButton QnMessageBox::standardButton(QAbstractButton* button) const
{
    return ui->buttonBox->standardButton(button);
}

QPushButton* QnMessageBox::button(QDialogButtonBox::StandardButton which) const
{
    return ui->buttonBox->button(which);
}

QAbstractButton *QnMessageBox::defaultButton() const
{
    Q_D(const QnMessageBox);
    return d->defaultButton;
}

void QnMessageBox::setDefaultButton(QAbstractButton* button, Qn::ButtonAccent accent)
{
    NX_ASSERT(ui->buttonBox->buttons().contains(button));
    if (!ui->buttonBox->buttons().contains(button))
        return;

    Q_D(QnMessageBox);
    if (button == d->defaultButton)
        return;

    if (QPushButton* pushButton = qobject_cast<QPushButton*>(button))
        pushButton->setDefault(true);

    if (button)
        button->setFocus();

    d->buttonAccent = accent;
    d->defaultButton = button;
    d->buttonDetection &= ~int(QnButtonDetection::DefaultButton);
    d->stylizeButtons();
}

void QnMessageBox::setDefaultButton(
    QDialogButtonBox::StandardButton button,
    Qn::ButtonAccent accent)
{
    if (button == QDialogButtonBox::NoButton)
        return;

    QPushButton* buttonWidget = ui->buttonBox->button(button);
    NX_ASSERT(buttonWidget, "Non-existing default button");

    if (buttonWidget)
        setDefaultButton(buttonWidget, accent);
}

QAbstractButton* QnMessageBox::escapeButton() const
{
    Q_D(const QnMessageBox);
    return d->escapeButton;
}

void QnMessageBox::setEscapeButton(QAbstractButton* button)
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
            return qnSkin->pixmap("standard_icons/message_box_success.png");
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

Qt::TextFormat QnMessageBox::informativeTextFormat() const
{
    Q_D(const QnMessageBox);
    return d->informativeTextFormat;
}

void QnMessageBox::setInformativeTextFormat(Qt::TextFormat format)
{
    Q_D(QnMessageBox);

    if (d->informativeTextFormat == format)
        return;

    for (auto& label: d->informativeLabels)
        label->setTextFormat(format);
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
        label->setOpenExternalLinks(true);
        label->setWordWrap(true);
        label->setText(line);
        ui->verticalLayout->insertWidget(index, label);
        d->informativeLabels.append(label);
        ++index;
    }

    for (auto& label: d->informativeLabels)
        label->setTextFormat(d->informativeTextFormat);
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
                ui->verticalLayout->indexOf(ui->checkBoxWidget),
                widget,
                stretch,
                alignment
            );
        case QnMessageBox::Layout::AfterMainLabel:
            ui->verticalLayout->insertWidget(
                ui->verticalLayout->indexOf(ui->mainLabel) + 1,
                widget,
                stretch,
                alignment);

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

void QnMessageBox::setCustomCheckBoxText(const QString& text)
{
    ui->checkBox->setText(text);
    setCheckBoxEnabled(!text.isEmpty());
}

bool QnMessageBox::isCheckBoxEnabled() const
{
    return !ui->checkBoxWidget->isHidden();
}

void QnMessageBox::setCheckBoxEnabled(bool value)
{
    ui->checkBoxWidget->setVisible(value);
}

bool QnMessageBox::isChecked() const
{
    return ui->checkBox->isChecked();
}

void QnMessageBox::setChecked(bool checked)
{
    ui->checkBox->setChecked(checked);
}

void QnMessageBox::setButtonAutoDetection(QnButtonDetection detection)
{
    Q_D(QnMessageBox);
    d->buttonDetection = detection;
}

int QnMessageBox::exec()
{
    Q_D(QnMessageBox);

    if (d->buttonDetection.testFlag(QnButtonDetection::DefaultButton))
        d->detectDefaultButton();

    d->detectEscapeButton();
    if (d->buttonDetection)
        d->stylizeButtons();
    NX_ASSERT(d->escapeButton);

    if (d->informativeLabels.isEmpty() && d->customWidgets.isEmpty() && !isCheckBoxEnabled())
    {
        ui->verticalLayout->removeItem(ui->verticalSpacer);
        delete ui->verticalSpacer;
    }

    adjustSize();

    base_type::exec();

    return d->execReturnCode(clickedButton());
}

void QnMessageBox::closeEvent(QCloseEvent* event)
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
            d->escapeButton->click();
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
        for (auto pb: ui->buttonBox->buttons())
        {
            buttonTexts += pb->text() + lit("   ");
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
            for (auto pb: ui->buttonBox->buttons())
            {
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
    auto size = sizeHint();

    size.setWidth(std::max({ kMinimumWidth, size.width(), minimumSizeHint().width() }));
    if (hasHeightForWidth())
        size.setHeight(heightForWidth(size.width()));

    setFixedSize(size);
}
