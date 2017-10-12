#include "login_dialog.h"
#include "ui_login_dialog.h"

#include <QtCore/QEvent>
#include <QtCore/QDir>

#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>

#include <api/app_server_connection.h>
#include <api/session_manager.h>
#include <api/model/connection_info.h>

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource/resource.h>

#include <nx/vms/discovery/manager.h>
#include <network/networkoptixmodulerevealcommon.h>
#include <network/system_helpers.h>

#include <nx/utils/raii_guard.h>
#include <nx/network/cloud/address_resolver.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/socket_global.h>
#include <rest/server/json_rest_result.h>
#include <client_core/client_core_settings.h>

#include <utils/applauncher_utils.h>
#include <utils/common/app_info.h>
#include <utils/common/url.h>
#include <utils/common/util.h>
#include <utils/connection_diagnostics_helper.h>

#include <plugins/resource/avi/avi_resource.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <plugins/resource/avi/filetypesupport.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/dialogs/connection_name_dialog.h>
#include <ui/dialogs/connection_testing_dialog.h>
#include <ui/graphics/items/resource/decodedpicturetoopengluploadercontextpool.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/widgets/rendering_widget.h>
#include <ui/workaround/gl_widget_factory.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <helpers/system_helpers.h>

using namespace nx::client::desktop::ui;

namespace {

static const QnUuid kCustomConnectionLocalId;

constexpr int kMinIntroWidth = 550;

static std::array<const char*, 5> kIntroNames {
    "intro.mkv",
    "intro.avi",
    "intro.png",
    "intro.jpg",
    "intro.jpeg"
};

void setEnabled(const QObjectList &objects, QObject *exclude, bool enabled)
{
    for (QObject *object: objects)
    {
        if (object != exclude)
        {
            if (QWidget *widget = qobject_cast<QWidget *>(object))
                widget->setEnabled(enabled);
        }
    }
}

QStandardItem* newConnectionItem(const QString& text, const QUrl& url, bool isValid = true)
{
    if (url.isEmpty())
        return nullptr;

    auto result = new QStandardItem(text);
    result->setData(url, Qn::UrlRole);
    if (!isValid)
        result->setData(QBrush(qnGlobals->errorTextColor()), Qt::TextColorRole);

    return result;
}

bool haveToStorePassword(const QnUuid& localId, const QUrl& url)
{
    /**
     * At first we check if we have stored connection to same system
     * with specified user and it stores password.
     */

    using namespace nx::client::core::helpers;

    if (getCredentials(localId, url.userName()).isValid())
        return true;

    /**
     * Looking for saved connections in login dialog (used till 2.6).
     * If we find connection with same host/user we can store password too.
     */
    const auto custom = qnSettings->customConnections();
    const auto itCustom = std::find_if(custom.begin(), custom.end(),
        [host = url.host(), port = url.port(), userName = url.userName()]
            (const QnConnectionData& data)
        {
            return (data.isCustom()
                && (data.url.host() == host)
                && (data.url.port() == port)
                && (data.url.userName() == userName));
        });

    const bool savedConnectionFound = (itCustom != custom.end());
    return savedConnectionFound;
}

struct AutoFoundSystemViewModel
{
    QString title;
    QUrl url;
    bool isValid = true;

    bool operator<(const AutoFoundSystemViewModel& other) const
    {
        return QString::compare(title, other.title, Qt::CaseInsensitive) < 0;
    }
};

} // namespace

/************************************************************************/
/* QnFoundSystemData                                                             */
/************************************************************************/

bool QnLoginDialog::QnFoundSystemData::operator==(const QnFoundSystemData& other) const
{
    return
        url == other.url
        && info.id == other.info.id
        && info.version == other.info.version
        && info.protoVersion == other.info.protoVersion
        && info.systemName == other.info.systemName
        ;
}

bool QnLoginDialog::QnFoundSystemData::operator!=(const QnFoundSystemData& other) const
{
    return !(*this == other);
}

/************************************************************************/
/* QnLoginDialog                                                        */
/************************************************************************/

QnLoginDialog::QnLoginDialog(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::LoginDialog),
    m_requestHandle(-1),
    m_renderingWidget(QnGlWidgetFactory::create<QnRenderingWidget>())
{
    ui->setupUi(this);

    setAccentStyle(ui->buttonBox->button(QDialogButtonBox::Ok));

    setWindowTitle(tr("Connect to Server..."));
    setHelpTopic(this, Qn::Login_Help);

    m_renderingWidget->setEffectiveWidth(kMinIntroWidth);

    QHBoxLayout* bbLayout = dynamic_cast<QHBoxLayout*>(ui->buttonBox->layout());
    NX_ASSERT(bbLayout);
    if (bbLayout)
    {
        QLabel* versionLabel = new QLabel(ui->buttonBox);
        versionLabel->setText(tr("Version %1").arg(QnAppInfo::applicationVersion()));
        QFont font = versionLabel->font();
        font.setPointSize(7);
        versionLabel->setFont(font);
        bbLayout->insertWidget(0, versionLabel);
    }

    QString introPath;
    for (auto name: kIntroNames)
    {
        introPath = qnSkin->path(name);
        if (!introPath.isEmpty())
            break;
    }

    QnAviResourcePtr resource = QnAviResourcePtr(new QnAviResource(lit("qtfile://") + introPath));
    if (FileTypeSupport::isImageFileExt(introPath))
        resource->addFlags(Qn::still_image);

    m_renderingWidget->setResource(resource);

    QVBoxLayout* layout = new QVBoxLayout(ui->videoSpacer);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_renderingWidget);
    DecodedPictureToOpenGLUploaderContextPool::instance()->ensureThereAreContextsSharedWith(m_renderingWidget);

    m_connectionsModel = new QStandardItemModel(this);
    ui->connectionsComboBox->setModel(m_connectionsModel);

    m_lastUsedItem = NULL;
    m_savedSessionsItem = new QStandardItem(tr("Saved Sessions"));
    m_savedSessionsItem->setFlags(Qt::ItemIsEnabled);
    m_autoFoundItem = new QStandardItem(tr("Auto-Discovered Servers"));
    m_autoFoundItem->setFlags(Qt::ItemIsEnabled);

    m_connectionsModel->appendRow(m_savedSessionsItem);
    m_connectionsModel->appendRow(m_autoFoundItem);

    connect(ui->connectionsComboBox, &QnTreeComboBox::currentIndexChanged, this,
        &QnLoginDialog::at_connectionsComboBox_currentIndexChanged);
    connect(ui->testButton, &QPushButton::clicked, this,
        &QnLoginDialog::at_testButton_clicked);
    connect(ui->saveButton, &QPushButton::clicked, this,
        &QnLoginDialog::at_saveButton_clicked);
    connect(ui->deleteButton, &QPushButton::clicked, this,
        &QnLoginDialog::at_deleteButton_clicked);
    connect(ui->passwordLineEdit, &QLineEdit::textChanged, this,
        &QnLoginDialog::updateAcceptibility);
    connect(ui->loginLineEdit, &QLineEdit::textChanged, this,
        &QnLoginDialog::updateAcceptibility);
    connect(ui->hostnameLineEdit, &QLineEdit::textChanged, this,
        &QnLoginDialog::updateAcceptibility);
    connect(ui->portSpinBox, QnSpinboxIntValueChanged, this,
        &QnLoginDialog::updateAcceptibility);

    resetConnectionsModel();
    updateFocus();

    /* Should be done after model resetting to avoid state loss. */
    ui->autoLoginCheckBox->setChecked(qnSettings->autoLogin());

    commonModule()->moduleDiscoveryManager()->onSignals(this,
        &QnLoginDialog::at_moduleChanged,
        &QnLoginDialog::at_moduleChanged,
        &QnLoginDialog::at_moduleLost);
}

QnLoginDialog::~QnLoginDialog()
{
}

void QnLoginDialog::updateFocus()
{
    ui->passwordLineEdit->setFocus();
    ui->passwordLineEdit->selectAll();
}

QUrl QnLoginDialog::currentUrl() const
{
    QUrl url;
    url.setScheme(lit("http"));
    url.setHost(ui->hostnameLineEdit->text().trimmed());
    url.setPort(ui->portSpinBox->value());
    url.setUserName(ui->loginLineEdit->text().trimmed());
    url.setPassword(ui->passwordLineEdit->text());
    return url;
}

bool QnLoginDialog::isValid() const
{
    QUrl url = currentUrl();
    return !ui->passwordLineEdit->text().isEmpty()
        && !ui->loginLineEdit->text().trimmed().isEmpty()
        && !ui->hostnameLineEdit->text().trimmed().isEmpty()
        && ui->portSpinBox->value() != 0
        && url.isValid()
        && !url.host().isEmpty();
}

void QnLoginDialog::accept()
{
    NX_EXPECT(isValid());
    if (!isValid())
        return;

    QUrl url = currentUrl();
	const auto guard = QPointer<QnLoginDialog>(this);
    m_requestHandle = qnClientCoreModule->connectionFactory()->testConnection(
        url, this,
        [this, url, guard](int handle, ec2::ErrorCode errorCode, const QnConnectionInfo &connectionInfo)
        {
            if (!guard)
                return;
            if (m_requestHandle != handle)
                return; //connect was cancelled


            const auto status = QnConnectionDiagnosticsHelper::validateConnection(
                connectionInfo, errorCode, this);

            if (!guard)
                return;

            m_requestHandle = -1;
            updateUsability();

            switch (status)
            {
                case Qn::SuccessConnectionResult:
                {
                    // In most cases we will connect succesfully by this url. Sow we can store it.

                    const bool autoLogin = ui->autoLoginCheckBox->isChecked();
                    QUrl lastUrlForLoginDialog = url;
                    if (!autoLogin)
                        lastUrlForLoginDialog.setPassword(QString());

                    qnSettings->setLastLocalConnectionUrl(lastUrlForLoginDialog);
                    qnSettings->save();

                    const bool storePasswordForTile =
                        haveToStorePassword(connectionInfo.localSystemId, url) || autoLogin;

                    action::Parameters params;
                    params.setArgument(Qn::UrlRole, url);
                    params.setArgument(Qn::StoreSessionRole, true);
                    params.setArgument(Qn::AutoLoginRole, autoLogin);
                    params.setArgument(Qn::StorePasswordRole, storePasswordForTile);
                    params.setArgument(Qn::ForceRole, true);
                    menu()->trigger(action::ConnectAction, params);

                    break;
                }
                case Qn::IncompatibleProtocolConnectionResult:
                case Qn::IncompatibleCloudHostConnectionResult:
                    menu()->trigger(action::DelayedForcedExitAction);
                    break; // to avoid cycle
                default:    //error
                    return;
            }

            base_type::accept();
        });

    updateUsability();
}

void QnLoginDialog::reject()
{
    if (m_requestHandle == -1)
    {
        base_type::reject();
        return;
    }

    m_requestHandle = -1;
    updateUsability();
}

void QnLoginDialog::changeEvent(QEvent *event)
{
    base_type::changeEvent(event);

    switch (event->type())
    {
        case QEvent::LanguageChange:
            ui->retranslateUi(this);
            break;
        default:
            break;
    }
}

void QnLoginDialog::showEvent(QShowEvent *event)
{
    base_type::showEvent(event);

    bool autoLogin = qnSettings->autoLogin();
    resetConnectionsModel();
    ui->autoLoginCheckBox->setChecked(autoLogin);

#ifdef Q_OS_MAC
    if (focusWidget())
        focusWidget()->activateWindow();
#endif
}

void QnLoginDialog::hideEvent(QHideEvent *event)
{
    base_type::hideEvent(event);
    m_renderingWidget->stopPlayback();
}

QString QnLoginDialog::defaultLastUsedConnectionName()
{
    return tr("* Last used connection *");
}

QString QnLoginDialog::deprecatedLastUsedConnectionName()
{
    return lit("* Last used connection *");
}

void QnLoginDialog::resetConnectionsModel()
{
    if (m_lastUsedItem != NULL)
        m_connectionsModel->removeRow(0); //last-used-connection row

    QModelIndex selectedIndex;

    const auto url = qnSettings->lastLocalConnectionUrl();
    m_lastUsedItem = (url.host().isEmpty()
        ? nullptr
        : ::newConnectionItem(defaultLastUsedConnectionName(), url));

    if (m_lastUsedItem != NULL)
    {
        m_connectionsModel->insertRow(0, m_lastUsedItem);
        selectedIndex = m_connectionsModel->index(0, 0);
    }

    resetSavedSessionsModel();
    resetAutoFoundConnectionsModel();

    /* Last used connection if exists, else last saved connection. */
    if (!selectedIndex.isValid())
        selectedIndex = m_connectionsModel->index(0, 0, m_savedSessionsItem->index());
    ui->connectionsComboBox->setCurrentIndex(selectedIndex);
    at_connectionsComboBox_currentIndexChanged(selectedIndex);
}

void QnLoginDialog::resetSavedSessionsModel()
{
    auto customConnections = qnSettings->customConnections();
    if (!m_lastUsedItem || customConnections.isEmpty())
    {
        QUrl url;
        url.setPort(DEFAULT_APPSERVER_PORT);
        url.setHost(QLatin1Literal(DEFAULT_APPSERVER_HOST));
        url.setUserName(helpers::kFactorySystemUser);

        customConnections.append(QnConnectionData(lit("default"), url, kCustomConnectionLocalId));
    }

    m_savedSessionsItem->removeRows(0, m_savedSessionsItem->rowCount());

    std::sort(customConnections.begin(), customConnections.end(),
        [](const QnConnectionData& left, const QnConnectionData& right)
        {
            return QString::compare(left.name, right.name, Qt::CaseInsensitive) < 0;
        });

    for (const auto& connection : customConnections)
    {
        NX_ASSERT(!connection.name.isEmpty());
        if (connection.name == deprecatedLastUsedConnectionName())
        {
            /**
              * Client with version which is less than 3.0 stores last used connection in custom
              * connections. To prevent placing two "* Last Used Connection *" items in the
              * connections combobox we have to filter out this item from saved connections.
              * See VMS-5889.
              */
            continue;
        }
        m_savedSessionsItem->appendRow(newConnectionItem(connection));
    }
}

void QnLoginDialog::resetAutoFoundConnectionsModel()
{
    m_autoFoundItem->removeRows(0, m_autoFoundItem->rowCount());
    if (m_foundSystems.size() == 0)
    {
        QStandardItem* noLocalEcs = new QStandardItem(L'<' + tr("none") + L'>');
        noLocalEcs->setFlags(Qt::ItemIsEnabled);
        m_autoFoundItem->appendRow(noLocalEcs);
        return;
    }

    QList<AutoFoundSystemViewModel> viewModels;
    viewModels.reserve(m_foundSystems.size());

    for (const QnFoundSystemData& data : m_foundSystems)
    {
        auto compatibilityCode = QnConnectionValidator::validateConnection(data.info);

        /* Do not show servers with incompatible customization or cloud host */
        if (!qnRuntime->isDevMode()
            && compatibilityCode == Qn::IncompatibleInternalConnectionResult)
        {
            continue;
        }

        AutoFoundSystemViewModel vm;
        vm.url = data.url;
        vm.isValid = (compatibilityCode == Qn::SuccessConnectionResult);

        if (!data.info.systemName.isEmpty())
        {
            vm.title = lit("%3 - (%1:%2)")
                .arg(vm.url.host()).arg(vm.url.port()).arg(data.info.systemName);
        }
        else
        {
            vm.title = lit("%1:%2").arg(vm.url.host()).arg(vm.url.port());
        }

        if (!vm.isValid)
        {
            const bool showBuild = compatibilityCode == Qn::IncompatibleProtocolConnectionResult
                || compatibilityCode == Qn::IncompatibleCloudHostConnectionResult;

            auto versionFormat = showBuild
                ? QnSoftwareVersion::FullFormat
                : QnSoftwareVersion::BugfixFormat;

            vm.title += lit(" (v%1)")
                .arg(data.info.version.toString(versionFormat));
        }

        viewModels.push_back(vm);
    }

    std::sort(viewModels.begin(), viewModels.end());
    for (const auto& vm: viewModels)
    {
        auto item = ::newConnectionItem(vm.title, vm.url, vm.isValid);
        m_autoFoundItem->appendRow(item);
    }


}

void QnLoginDialog::updateAcceptibility()
{
    bool acceptable = isValid();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(acceptable);
    ui->testButton->setEnabled(acceptable);
    ui->saveButton->setEnabled(acceptable);
}

void QnLoginDialog::updateUsability()
{
    if (m_requestHandle == -1)
    {
        ::setEnabled(children(), ui->buttonBox, true);
        ::setEnabled(ui->buttonBox->children(), ui->buttonBox->button(QDialogButtonBox::Cancel), true);
        unsetCursor();
        ui->buttonBox->button(QDialogButtonBox::Cancel)->unsetCursor();

        updateAcceptibility();
        updateFocus();
    }
    else
    {
        ::setEnabled(children(), ui->buttonBox, false);
        ::setEnabled(ui->buttonBox->children(), ui->buttonBox->button(QDialogButtonBox::Cancel), false);
        setCursor(Qt::BusyCursor);
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setCursor(Qt::ArrowCursor);
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnLoginDialog::at_connectionsComboBox_currentIndexChanged(const QModelIndex &index)
{
    QStandardItem* item = m_connectionsModel->itemFromIndex(index);
    QUrl url = item == NULL ? QUrl() : item->data(Qn::UrlRole).toUrl();
    ui->hostnameLineEdit->setText(url.host());
    ui->portSpinBox->setValue(url.port());
    ui->loginLineEdit->setText(url.userName().isEmpty()
        ? helpers::kFactorySystemUser  // 99% of users have only one login - admin
        : url.userName());
    ui->passwordLineEdit->setText(url.password());

    const auto currentConnection = ui->connectionsComboBox->currentData(
        Qn::ConnectionInfoRole).value<QnConnectionData>();
    ui->deleteButton->setEnabled(currentConnection != QnConnectionData());
    ui->autoLoginCheckBox->setChecked(false);
    updateFocus();
}

void QnLoginDialog::at_testButton_clicked()
{
    NX_EXPECT(isValid());
    if (!isValid())
        return;

    bool connectRequested = false;

    QScopedPointer<QnConnectionTestingDialog> dialog(new QnConnectionTestingDialog(this));
    connect(dialog.data(), &QnConnectionTestingDialog::connectRequested, this,
        [&connectRequested]
        {
            connectRequested = true;
        });

    dialog->testConnection(currentUrl());
    dialog->exec();

    updateFocus();
    if (connectRequested)
        accept();
}

QStandardItem* QnLoginDialog::newConnectionItem(const QnConnectionData& connection)
{
    if (connection == QnConnectionData())
        return nullptr;

    const auto text = (!connection.name.isEmpty() ? connection.name
        : tr("%1 at %2").arg(connection.url.userName(), connection.url.host()));

    auto result = ::newConnectionItem(text, connection.url);
    result->setData(QVariant::fromValue(connection), Qn::ConnectionInfoRole);
    return result;
}

void QnLoginDialog::at_saveButton_clicked()
{
    NX_EXPECT(isValid());
    if (!isValid())
        return;

    QString name = tr("%1 at %2").arg(ui->loginLineEdit->text()).arg(ui->hostnameLineEdit->text());
    bool savePassword = !ui->passwordLineEdit->text().isEmpty();
    {
        QScopedPointer<QnConnectionNameDialog> dialog(new QnConnectionNameDialog(this));
        dialog->setName(name);
        dialog->setSavePasswordEnabled(savePassword);
        dialog->setSavePassword(savePassword);

        if (!dialog->exec())
            return;

        name = dialog->name();
        savePassword &= dialog->savePassword();
    }

    // save here because the 'connections' field is modifying below
    QString password = ui->passwordLineEdit->text();
    bool autoLogin = qnSettings->autoLogin();

    auto connections = qnSettings->customConnections();
    if (connections.contains(name))
    {
        QnMessageBox dialog(QnMessageBoxIcon::Question,
            tr("Overwrite existing connection?"),
            tr("There is another connection with the same name."),
            QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, this);

        dialog.addCustomButton(QnMessageBoxCustomButton::Overwrite,
            QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
        if (dialog.exec() == QDialogButtonBox::Cancel)
            return;

        connections.removeOne(name);
    }

    const QUrl url = currentUrl();
    auto connectionData = QnConnectionData(name, url, kCustomConnectionLocalId);

    if (!savePassword)
        connectionData.url.setPassword(QString());
    connections.prepend(connectionData);
    qnSettings->setCustomConnections(connections);
    qnSettings->save();

    resetSavedSessionsModel();

    m_connectionsModel->rowCount();

    const auto idx = getModelIndexForName(connectionData.name);
    ui->connectionsComboBox->setCurrentIndex(idx);
    at_connectionsComboBox_currentIndexChanged(idx); // call directly in case index change will not work
    ui->passwordLineEdit->setText(password);         // password is cleared on index change
    ui->autoLoginCheckBox->setChecked(autoLogin);
}

QModelIndex QnLoginDialog::getModelIndexForName(const QString& name)
{
    const auto savedSessionsIndex = m_savedSessionsItem->index();
    const auto rowCount = m_connectionsModel->rowCount(savedSessionsIndex);
    for (auto row = 0; row != rowCount; ++row)
    {
        const auto index = m_connectionsModel->index(row, 0, savedSessionsIndex);
        const auto savedName = m_connectionsModel->data(index, Qt::DisplayRole);
        if (savedName == name)
            return index;
    }

    return QModelIndex();
}

void QnLoginDialog::at_deleteButton_clicked()
{
    auto connections = qnSettings->customConnections();

    auto connection = ui->connectionsComboBox->currentData(
        Qn::ConnectionInfoRole).value<QnConnectionData>();
    if (!connections.contains(connection.name))
        return;

    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Delete connection?"), connection.name,
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        this);

    dialog.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    using namespace nx::client::core::helpers;

    removeCredentials(connection.localId, connection.url.userName());
    connections.removeOne(connection.name);
    qnSettings->setCustomConnections(connections);
    qnSettings->save();
    resetConnectionsModel();
}

void QnLoginDialog::at_moduleChanged(nx::vms::discovery::ModuleEndpoint module)
{
    auto isCloudAddress =
        [](const HostAddress& address) -> bool
        {
            return nx::network::SocketGlobals::addressResolver()
                .isCloudHostName(address.toString());
        };

    auto isLoopback =
        [](const HostAddress& address) -> bool
        {
            return QHostAddress(address.toString()).isLoopback();
        };

    bool loopback = false;
    if (!isCloudAddress(module.endpoint.address))
    {
        loopback = isLoopback(module.endpoint.address);
    }
    else
    {
        at_moduleLost(module.id);
        return;
    }

    QnFoundSystemData data;
    data.info = module;
    data.url.setScheme(lit("http"));
    data.url.setHost(module.endpoint.address.toString());
    data.url.setPort(module.endpoint.port);

    if (m_foundSystems.contains(module.id))
    {
        QnFoundSystemData& oldData = m_foundSystems[module.id];
        if (oldData != data)
        {
            oldData = data;
            resetAutoFoundConnectionsModel();
        }
    }
    else
    {
        m_foundSystems.insert(module.id, data);
        resetAutoFoundConnectionsModel();
    }
}

void QnLoginDialog::at_moduleLost(QnUuid id)
{
    if (m_foundSystems.remove(id))
        resetAutoFoundConnectionsModel();
}
