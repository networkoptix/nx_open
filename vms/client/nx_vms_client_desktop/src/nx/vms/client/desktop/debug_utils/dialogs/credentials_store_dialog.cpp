// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "credentials_store_dialog.h"
#include "ui_credentials_store_dialog.h"

#include <QtGui/QStandardItemModel>
#include <QtWidgets/QPushButton>

#include <finders/systems_finder.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/keychain_property_storage_backend.h>
#include <nx/vms/client/desktop/application_context.h>
#include <ui/workbench/workbench_context.h>

#include "../utils/debug_custom_actions.h"

namespace nx::vms::client::desktop {

namespace {

const auto SystemIdRole = Qt::UserRole + 1;
const nx::utils::log::Filter kKeychainFilter(
    nx::utils::log::Tag(typeid(nx::vms::client::core::KeychainBackend)));

} // namespace

struct CredentialsStoreDialog::Private
{
    QScopedPointer<QStandardItemModel> model;

    Private()
    {
        model.reset(new QStandardItemModel());
        rebuildModel();
    }

    void rebuildModel()
    {
        model->clear();
        const auto& data = appContext()->coreSettings()->systemAuthenticationData();
        for (auto [systemId, credentialsList]: data)
        {
            const auto system = qnSystemsFinder->getSystem(systemId.toString());
            QString title = system ? system->name() : systemId.toSimpleString();

            QStandardItem* item = new QStandardItem(title);
            item->setData(QVariant::fromValue(systemId), SystemIdRole);
            model->appendRow(item);
            for (const auto& creds: credentialsList)
            {
                QStandardItem* login = new QStandardItem(QString::fromStdString(creds.user));
                login->setData(QVariant::fromValue(systemId), SystemIdRole);
                item->appendRow(login);
            }
        }
    }
};

CredentialsStoreDialog::CredentialsStoreDialog(QWidget *parent):
    base_type(parent),
    d(new Private()),
    ui(new Ui::CredentialsStoreDialog())
{
    ui->setupUi(this);
    ui->treeView->setModel(d->model.data());

    using namespace nx::vms::client::core;

    const auto addButton = new QPushButton("Add", this);
    connect(addButton, &QAbstractButton::clicked, this,
        [this]()
        {
            using namespace nx::network::http;

            const QnUuid randomSystemId = QnUuid::createUuid();
            Credentials emptyCredentials("empty", {});
            PasswordCredentials passwordCredentials("password", "password");
            Credentials ha1Credentials("ha1", Ha1AuthToken("ha1"));
            Credentials tokenCredentials("token", BearerAuthToken("token"));

            CredentialsManager::storeCredentials(randomSystemId, emptyCredentials);
            CredentialsManager::storeCredentials(randomSystemId, passwordCredentials);
            CredentialsManager::storeCredentials(randomSystemId, ha1Credentials);
            CredentialsManager::storeCredentials(randomSystemId, tokenCredentials);
            d->rebuildModel();
        });
    ui->buttonBox->addButton(addButton, QDialogButtonBox::ButtonRole::HelpRole);

    const auto removeButton = new QPushButton("Remove", this);
    connect(removeButton, &QAbstractButton::clicked, this,
        [this]()
        {
            auto selectionModel = this->ui->treeView->selectionModel();
            if (!selectionModel->hasSelection())
                return;

            for (const auto& idx: selectionModel->selectedIndexes())
            {
                const auto systemId = idx.data(SystemIdRole).value<QnUuid>();
                CredentialsManager::removeCredentials(systemId);
            }
            d->rebuildModel();
        });
    ui->buttonBox->addButton(removeButton, QDialogButtonBox::ButtonRole::HelpRole);

    using namespace nx::utils::log;
    addLogger(std::make_unique<Logger>(std::set<Filter>{kKeychainFilter}, Level::verbose,
        std::make_unique<StdOut>()));
}

CredentialsStoreDialog::~CredentialsStoreDialog()
{
}

void CredentialsStoreDialog::registerAction()
{
    registerDebugAction(
        "Credentials",
        [](QnWorkbenchContext* context)
        {
            auto dialog = std::make_unique<CredentialsStoreDialog>(context->mainWindowWidget());
            dialog->exec();
        });
}

} // namespace nx::vms::client::desktop
