// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

/**
 *  Base class for creating paged preferences dialogs (where each page is an instance of
 *  `QnAbstractPreferencesWidget` class).
 *
 *  Derived pages basically should:
 *  * Implement loadDataToUi / hasChanges / applyChanges methods.
 *  * Emit hasChangesChanged in all cases where new changes can appear.
 */
class QnAbstractPreferencesWidget: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    using base_type = QWidget;
public:
    explicit QnAbstractPreferencesWidget(QWidget* parent = nullptr);

    /**
     * Check if there are modified values.
     * @return Whether something was changed.
     */
    virtual bool hasChanges() const { return false; };

    /**
     * Read widget elements' values from model data or directly from the server.
     */
    virtual void loadDataToUi() {};

    /**
     * Save widget elements' values to model data or directly to the server.
     */
    virtual void applyChanges() {};

    /**
     * Discard unsaved changes. Here widget should cancel all network requests if they are running.
     * Please make sure `isNetworkRequestRunning()` will return false after this call.
     */
    virtual void discardChanges() {}

    /**
     * Check that all values are correct and saving is possible.
     * @return Whether changes can be applied.
     */
    virtual bool canApplyChanges() const { return true; }

    /** Whether widget has at least one running network request. */
    virtual bool isNetworkRequestRunning() const { return false; }

    /**
     * Activate sub-element denoted by the URL.
     * @return True if element is found and activated.
     */
    virtual bool activate(const QUrl& /*url*/) { return true; }

    /**
     * Update ui strings (if required).
     */
    virtual void retranslateUi();

    /** Whether widget is in read-only state. */
    bool isReadOnly() const;

    /** Mark widget as read-only (e.g. if the system is in read-only state). */
    void setReadOnly(bool readOnly);

    /** Hide all runtime warnings about proposed changes. */
    virtual void resetWarnings() {}

signals:
    /** Signal must be emitted whenever hasChanges() is potentially changed. */
    void hasChangesChanged();

protected:
    virtual void setReadOnlyInternal(bool /*readOnly*/) {}

private:
    bool m_readOnly = false;
};
