#include "credentials_store_dialog.h"
#include "ui_credentials_store_dialog.h"

#include <QtGui/QStandardItemModel>
#include <QtWidgets/QPushButton>

#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/client/core/settings/keychain_property_storage_backend.h>

#include <finders/systems_finder.h>
#include <ui/workbench/workbench_context.h>

#include <nx/utils/log/log.h>

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
        const auto& data = core::settings()->systemAuthenticationData();
        for (const auto& systemId: data.keys())
        {
            const auto system = qnSystemsFinder->getSystem(systemId.toString());
            QString title = system ? system->name() : systemId.toSimpleString();

            QStandardItem* item = new QStandardItem(title);
            item->setData(qVariantFromValue(systemId), SystemIdRole);
            model->appendRow(item);
            for (const auto& creds: data[systemId])
            {
                QStandardItem* login = new QStandardItem(creds.user);
                login->setData(qVariantFromValue(systemId), SystemIdRole);
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

    const auto addButton = new QPushButton("Add", this);
    connect(addButton, &QAbstractButton::clicked, this,
        [this]()
        {
            auto data = core::settings()->systemAuthenticationData();
            data[QnUuid::createUuid()].push_back({"test", "test"});
            core::settings()->systemAuthenticationData = data;
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

            auto data = core::settings()->systemAuthenticationData();

            for (const auto& idx: selectionModel->selectedIndexes())
            {
                const auto systemId = idx.data(SystemIdRole).value<QnUuid>();
                data.remove(systemId);
            }
            core::settings()->systemAuthenticationData = data;
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

} // namespace nx::vms::client::desktop

