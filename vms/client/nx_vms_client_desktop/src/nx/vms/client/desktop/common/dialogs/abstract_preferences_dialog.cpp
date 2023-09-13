// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_preferences_dialog.h"

#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>

#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator_button.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

namespace nx::vms::client::desktop {

namespace {

void setBusy(QAbstractButton* button, bool on)
{
    if (auto busyIndicator = qobject_cast<BusyIndicatorButton*>(button))
        busyIndicator->showIndicator(on);
    button->setEnabled(!on);
}

} // namespace

AbstractPreferencesDialog::AbstractPreferencesDialog(QWidget* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags)
{
}

void AbstractPreferencesDialog::addPage(
    int key,
    QnAbstractPreferencesWidget* widget,
    const QString& title)
{
    base_type::addPage(key, widget, title);
    connect(widget, &QnAbstractPreferencesWidget::hasChangesChanged, this,
        &AbstractPreferencesDialog::updateButtonBox);
}

void AbstractPreferencesDialog::setCurrentPage(int key, const QUrl& url)
{
    base_type::setCurrentPage(key);
    if (currentPage() == key && !url.isEmpty())
    {
        for (const auto& page: preferencesPages())
        {
            if (page.key == key)
            {
                page.preferencesWidget->activate(url);
                break;
            }
        }
    }
}

bool AbstractPreferencesDialog::closeDialogWithConfirmation()
{
    if (isHidden())
        return true;

    if (!hasChanges())
    {
        reject();
        return true;
    }

    switch (showConfirmationDialog())
    {
        case QDialogButtonBox::Yes:
        case QDialogButtonBox::Apply:
        {
            accept();
            return true;
        }

        case QDialogButtonBox::No:
        case QDialogButtonBox::Discard:
        {
            reject();
            return true;
        }

        // Cancel was pressed.
        default:
        {
            break;
        }
    }

    return false;
}

void AbstractPreferencesDialog::accept()
{
    if (hasChanges())
    {
        if (!canApplyChanges())
            return;

        applyChanges();
    }

    for (const auto& page: preferencesPages())
        page.preferencesWidget->resetWarnings();

    // Make sure all network requests complete before dialog closing.
    setBusy(m_okButton, true);
    while (isNetworkRequestRunning())
        qApp->processEvents();
    setBusy(m_okButton, false);
    base_type::accept();
}

void AbstractPreferencesDialog::reject()
{
    // Make sure all network requests complete before dialog closing. Hide the dialog meanwhile.
    hide();
    discardChangesSync();
    base_type::reject();
}

void AbstractPreferencesDialog::showEvent(QShowEvent* event)
{
    loadDataToUi();
    base_type::showEvent(event);
}

void AbstractPreferencesDialog::retranslateUi()
{
    for (const auto& page: preferencesPages())
        page.preferencesWidget->retranslateUi();
}

void AbstractPreferencesDialog::loadDataToUi()
{
    for (const auto& page: preferencesPages())
        page.preferencesWidget->loadDataToUi();

    retranslateUi();
    updateButtonBox();
}

void AbstractPreferencesDialog::applyChanges()
{
    for (const auto& page: preferencesPages())
    {
        if (page.enabled && page.visible && page.preferencesWidget->hasChanges())
            page.preferencesWidget->applyChanges();
    }

    updateButtonBox();
}

void AbstractPreferencesDialog::discardChanges()
{
    for (const auto& page: preferencesPages())
    {
        page.preferencesWidget->discardChanges();
        page.preferencesWidget->resetWarnings();
        NX_ASSERT(!page.preferencesWidget->isNetworkRequestRunning());
    }
}

bool AbstractPreferencesDialog::canApplyChanges() const
{
    if (isReadOnly())
        return false;

    return ranges::all_of(preferencesPages(),
        [](const auto& page)
        { return !page.enabled || !page.visible || page.preferencesWidget->canApplyChanges(); });
}

bool AbstractPreferencesDialog::isNetworkRequestRunning() const
{
    return ranges::any_of(preferencesPages(),
        [](const auto& page) { return page.preferencesWidget->isNetworkRequestRunning(); });
}

bool AbstractPreferencesDialog::hasChanges() const
{
    if (isReadOnly())
        return false;

    return ranges::any_of(preferencesPages(),
        [](const auto& page)
        { return page.enabled && page.visible && page.preferencesWidget->hasChanges(); });
}

void AbstractPreferencesDialog::initializeButtonBox()
{
    base_type::initializeButtonBox();

    for (auto button: buttonBox()->buttons())
    {
        switch (buttonBox()->buttonRole(button))
        {
            case QDialogButtonBox::ButtonRole::AcceptRole:
                m_okButton = button;
                break;
            case QDialogButtonBox::ButtonRole::ApplyRole:
                m_applyButton = button;
                break;
            case QDialogButtonBox::ButtonRole::RejectRole:
                m_cancelButton = button;
                break;
        }
    }

    if (!m_okButton)
    {
        m_okButton = new BusyIndicatorButton(this);
        m_okButton->setText(tr("OK"));
        setAccentStyle(m_okButton);
        buttonBox()->addButton(m_okButton, QDialogButtonBox::AcceptRole);
    }

    if (!m_applyButton)
    {
        m_applyButton = new BusyIndicatorButton(this);
        m_applyButton->setText(tr("Apply"));
        buttonBox()->addButton(m_applyButton, QDialogButtonBox::ApplyRole);
    }

    NX_ASSERT(m_cancelButton, "Dynamic creation is not implemented yet");

    updateButtonBox();
}

void AbstractPreferencesDialog::buttonBoxClicked(QAbstractButton* button)
{
    base_type::buttonBoxClicked(button);

    if (button == m_applyButton && hasChanges() && canApplyChanges())
        applyChangesSync();
}

void AbstractPreferencesDialog::setReadOnlyInternal()
{
    for (const auto& page: preferencesPages())
        page.preferencesWidget->setReadOnly(isReadOnly());
}

void AbstractPreferencesDialog::updateButtonBox()
{
    // Function can be called before ui is initialized.
    if (!buttonBox())
        return;

    const bool changesPresent = hasChanges();
    const bool canApply = !changesPresent || canApplyChanges();
    const bool isNetworkRequestRunning = this->isNetworkRequestRunning();

    m_okButton->setEnabled(canApply && !isNetworkRequestRunning);
    m_applyButton->setEnabled(changesPresent && canApply && !isNetworkRequestRunning);
}

void AbstractPreferencesDialog::applyChangesSync()
{
    applyChanges();
    m_okButton->setEnabled(false);
    setBusy(m_applyButton, true);
    waitUntilNetworkRequestsComplete();
    setBusy(m_applyButton, false);
    updateButtonBox();
}

void AbstractPreferencesDialog::discardChangesSync()
{
    m_okButton->setEnabled(false);
    m_applyButton->setEnabled(false);
    discardChanges();
    waitUntilNetworkRequestsComplete();
    updateButtonBox();
}

AbstractPreferencesDialog::PreferencesPages AbstractPreferencesDialog::preferencesPages() const
{
    PreferencesPages result;
    for (const auto& page: pages())
    {
        if (auto preferencesWidget = qobject_cast<QnAbstractPreferencesWidget*>(page.widget))
        {
            PreferencesPage p(page);
            p.preferencesWidget = preferencesWidget;
            result.push_back(p);
        }
    }
    return result;
}

QDialogButtonBox::StandardButton AbstractPreferencesDialog::showConfirmationDialog()
{
    QStringList details;
    for (const auto& page: preferencesPages())
    {
        if (page.enabled && page.visible && page.preferencesWidget->hasChanges())
            details << "* " + page.title;
    }

    static const auto kDelimiter = '\n';
    const auto extraMessage = details.empty()
        ? QString()
        : tr("Unsaved changes:") + kDelimiter + details.join(kDelimiter);

    QnMessageBox messageBox(
        QnMessageBox::Icon::Question,
        tr("Save changes before exit?"),
        extraMessage,
        QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
        QDialogButtonBox::Yes,
        this);

    if (!canApplyChanges())
        messageBox.button(QDialogButtonBox::Yes)->setEnabled(false);

    return static_cast<QDialogButtonBox::StandardButton>(messageBox.exec());
}

void AbstractPreferencesDialog::waitUntilNetworkRequestsComplete()
{
    while (isNetworkRequestRunning())
        qApp->processEvents();
}

} // namespace nx::vms::client::desktop
