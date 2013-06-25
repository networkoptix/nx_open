#include "ptz_preset_dialog.h"
#include "ui_ptz_preset_dialog.h"

#include <QtGui/QKeyEvent>

#include <utils/common/variant.h>
#include <ui/common/palette.h>

QnPtzPresetDialog::QnPtzPresetDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::PtzPresetDialog()),
    m_key(-1),
    m_modifiers(0)
{
    ui->setupUi(this);

    connect(ui->nameEdit, SIGNAL(textChanged(const QString &)), this, SLOT(updateOkButtonEnabled()));

    ui->hotkeyLineEdit->installEventFilter(this);
    updateOkButtonEnabled();
}

QnPtzPresetDialog::~QnPtzPresetDialog() {
    return;
}

QnPtzPreset QnPtzPresetDialog::preset() const {
    QnPtzPreset result = m_preset;
    result.name = ui->nameEdit->text();
    if (m_key == 0)
        result.hotkey = QKeySequence();
    else
    if (m_key > 0)
        result.hotkey = QKeySequence(m_key + m_modifiers);

    return result;
}

void QnPtzPresetDialog::setPreset(const QnPtzPreset &preset) {
    m_preset = preset;
    ui->nameEdit->setText(preset.name);
    ui->hotkeyLineEdit->setText(preset.hotkey.toString(QKeySequence::NativeText));
}

const QList<QKeySequence> &QnPtzPresetDialog::forbiddenHotkeys() const {
    return m_forbiddenHotkeys;
}

void QnPtzPresetDialog::setForbiddenHotkeys(const QList<QKeySequence> &forbiddenHotkeys) {
    m_forbiddenHotkeys = forbiddenHotkeys;
    updateHotkey();
}

bool QnPtzPresetDialog::eventFilter(QObject *obj, QEvent *event) {
    if (obj != ui->hotkeyLineEdit)
        return base_type::eventFilter(obj, event);

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(event);
        if (pKeyEvent->key() == Qt::Key_Tab)
            return false;

        if (pKeyEvent->key() == Qt::Key_Escape && isHotkeyValid() ) {
            return false;
        }

        if (pKeyEvent->key() == Qt::Key_Escape || pKeyEvent->key() == Qt::Key_Delete || pKeyEvent->key() == Qt::Key_Backspace) {
            m_key = 0;
            m_modifiers = 0;
            updateHotkey();
            return true;
        }

        switch (pKeyEvent->key()) {
        case Qt::Key_Shift:
        case Qt::Key_Control:
        case Qt::Key_Alt:
        case Qt::Key_AltGr:
        case Qt::Key_Meta:
            m_key = 0;
            break;
        default:
            m_key = pKeyEvent->key();
        }

        m_modifiers = pKeyEvent->modifiers();
    } else if (event->type() == QEvent::KeyRelease) {
        QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(event);
        qDebug() << pKeyEvent->key();
        qDebug() << pKeyEvent->modifiers();
    } else
        return base_type::eventFilter(obj, event);

    updateHotkey();

    return true;

}

void QnPtzPresetDialog::updateOkButtonEnabled() {
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(isHotkeyValid() && !ui->nameEdit->text().isEmpty());
}

bool QnPtzPresetDialog::isHotkeyValid() const {
    return ui->hotkeyLineEdit->text().isEmpty() || m_key != 0;
}

void QnPtzPresetDialog::updateHotkey() {
    QKeySequence hotkey(m_key + m_modifiers);
    ui->hotkeyLineEdit->setText(hotkey.toString(QKeySequence::NativeText));

    if (m_forbiddenHotkeys.contains(hotkey) || m_key == 0)
        setPaletteColor(ui->hotkeyLineEdit, QPalette::Text, Qt::red);
    else
        setPaletteColor(ui->hotkeyLineEdit, QPalette::Text, this->palette().color(QPalette::Text));
    updateOkButtonEnabled();
}
