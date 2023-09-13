// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>

#include "generic_tabbed_dialog.h"

class QnAbstractPreferencesWidget;

namespace nx::vms::client::desktop {

/**
 *  Base class for creating paged preferences dialogs (where each page is an instance of
 *  `QnAbstractPreferencesWidget` class).
 *
 *  Derived dialogs basically should implement the following behavior model:
 *  * Create pages in constructor.
 *  * Call loadDataToUi method every time the source model changes.
 */
class AbstractPreferencesDialog: public GenericTabbedDialog
{
    Q_OBJECT
    using base_type = GenericTabbedDialog;

public:
    AbstractPreferencesDialog(QWidget* parent = nullptr, Qt::WindowFlags windowFlags = {});

    /**
     * Overload of the base method. Add page, associated with the given key and the following
     * title. Pages are displayed in the order of adding.
     */
    void addPage(int key, QnAbstractPreferencesWidget* widget, const QString& title);

    /**
     * Overload of the base method. Select current page by key, and if possible, pass further
     * details in form of url to the page.
     */
    void setCurrentPage(int key, const QUrl& url = {});

    /**
     * Check whether the dialog has unsaved changes, and if it has, ask user whether they should be
     * applied or discarded.
     * @return True if dialog was closed, false if user pressed Cancel.
     */
    bool closeDialogWithConfirmation();

    /**
     * Overriding default accept method. Here the dialog will try to apply all changes (if
     * possible). If any page cannot apply its changes, dialog will not be closed.
     */
    virtual void accept() override;

    /**
     * Overriding default reject method. Here the dialog will try to revert all changes (if any
     * were applied instantly). If any page cannot revert its changes, dialog will not be closed.
     * Otherwise dialog will be closed.
     */
    virtual void reject() override;

protected:
    /** Update dialog data on showing. */
    virtual void showEvent(QShowEvent* event) override;

    /**
     * Update dialog ui if source data was changed. Usually is overridden to update dialog title.
     * Must call parent method in the implementation.
     * TODO: #sivanov Actual method usage does not fit the name. Method should be removed.
     */
    virtual void retranslateUi();

    /**
     * Refresh ui state based on model data. Should be called from derived classes.
     */
    virtual void loadDataToUi();

    /**
     * Apply changes to model. Called when user presses OK or Apply buttons.
     */
    virtual void applyChanges();

    /**
     * Discard unsaved changes. Here dialog should cancel all network requests if they are running.
     * Please make sure `isNetworkRequestRunning()` will return false after this call.
     */
    virtual void discardChanges();

    /**
     * @return Whether all values are correct so saving is possible.
     */
    virtual bool canApplyChanges() const;

    /** Whether dialog has at least one running network request. */
    virtual bool isNetworkRequestRunning() const;

    /**
     * @return Whether there are any unsaved changes in the dialog.
     */
    virtual bool hasChanges() const;

    virtual void initializeButtonBox() override;

    virtual void buttonBoxClicked(QAbstractButton* button) override;

    virtual void setReadOnlyInternal() override;

    /** Update state of the dialog buttons. */
    virtual void updateButtonBox();

    /** Apply changes and wait until all network requests are completed. */
    void applyChangesSync();

    /** Discard changes and wait until all network requests are completed. */
    void discardChangesSync();

private:
    struct PreferencesPage: Page
    {
        PreferencesPage(const Page& base): Page(base) {}

        QnAbstractPreferencesWidget* preferencesWidget = nullptr;
    };
    using PreferencesPages = QList<PreferencesPage>;

    /** List of pages which are represented by QnAbstractPreferencesWidget instances. */
    PreferencesPages preferencesPages() const;

    /**
     * Show the dialog, asking what to do with unsaved changes.
     * @return QDialogButtonBox::Yes if the changes should be saved, QDialogButtonBox::No if the
     *     changes should be discarded, QDialogButtonBox::Cancel to abort the process.
     */
    QDialogButtonBox::StandardButton showConfirmationDialog();

    /**
     * Actively wait (process events) until all tabs complete their network requests.
     */
    void waitUntilNetworkRequestsComplete();

private:
    QAbstractButton* m_okButton = nullptr;
    QAbstractButton* m_applyButton = nullptr;
    QAbstractButton* m_cancelButton = nullptr;
};

} // namespace nx::vms::client::desktop
