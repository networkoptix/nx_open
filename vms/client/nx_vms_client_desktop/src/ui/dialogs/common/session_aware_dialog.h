// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>

#include <nx/vms/client/desktop/common/dialogs/abstract_preferences_dialog.h>
#include <ui/dialogs/common/button_box_dialog.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/message_box_helper.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/common/forward.h>

#define COMMA ,
#define ID(x) x

/**
 * Dialog that will be closed if user is disconnected from the system.
 * Note: class is WindowContextAware.
 */
template<class Base>
class QnSessionAware: public Base, public QnSessionAwareDelegate
{
    typedef Base base_type;
public:
    QN_FORWARD_CONSTRUCTOR(
        QnSessionAware, Base, ID(COMMA QnSessionAwareDelegate(this->parent()){}))

    virtual bool tryClose(bool force) override
    {
        base_type::reject();
        if (force)
            base_type::hide();
        return true;
    }
};

#undef ID
#undef COMMA

using QnSessionAwareMessageBox = QnMessageBoxHelper<QnSessionAware<QnMessageBox>>;
using QnSessionAwareDialog = QnSessionAware<QnDialog>;
using QnSessionAwareFileDialog = QnSessionAware<QnCustomFileDialog>;

/**
 * Button box dialog that will be closed if user is disconnected from the system.
 * Note: class is WindowContextAware.
 */
class QnSessionAwareButtonBoxDialog: public QnButtonBoxDialog, public QnSessionAwareDelegate
{
    Q_OBJECT;
    typedef QnButtonBoxDialog base_type;

public:
    QnSessionAwareButtonBoxDialog(QWidget *parent, Qt::WindowFlags windowFlags = {});

    virtual bool tryClose(bool force) override;
};

/**
 * Tabbed dialog that will be closed if user is disconnected from the system.
 * Note: class is WindowContextAware.
 */
class QnSessionAwareTabbedDialog:
    public nx::vms::client::desktop::AbstractPreferencesDialog,
    public QnSessionAwareDelegate
{
    Q_OBJECT;
    typedef nx::vms::client::desktop::AbstractPreferencesDialog base_type;

public:
    QnSessionAwareTabbedDialog(QWidget* parent, Qt::WindowFlags windowFlags = {});

    virtual bool tryClose(bool force) override;
};
