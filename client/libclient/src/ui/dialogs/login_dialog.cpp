#include "login_dialog.h"
#include "ui_login_dialog.h"

#include <QtCore/QEvent>
#include <QtCore/QDir>

#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>

#include <api/app_server_connection.h>
#include <api/session_manager.h>
#include <api/model/connection_info.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <common/common_module.h>

#include <core/resource/resource.h>

#include <ui/actions/action_manager.h>
#include <ui/dialogs/connection_name_dialog.h>
#include <ui/widgets/rendering_widget.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/gl_widget_factory.h>

#include <network/module_finder.h>
#include <network/networkoptixmodulerevealcommon.h>

#include <utils/applauncher_utils.h>
#include <utils/common/url.h>
#include <utils/common/util.h>

#include <plugins/resource/avi/avi_resource.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <plugins/resource/avi/filetypesupport.h>

#include "connection_testing_dialog.h"
#include "ui/graphics/items/resource/decodedpicturetoopengluploadercontextpool.h"

#include <ui/style/globals.h>

#include <utils/connection_diagnostics_helper.h>

#include <utils/common/app_info.h>
#include <ui/style/custom_style.h>
#include <nx/utils/raii_guard.h>
#include <nx/network/http/asynchttpclient.h>
#include <rest/server/json_rest_result.h>
#include <client_core/client_core_settings.h>
#include <ui/workaround/widgets_signals_workaround.h>


namespace {
static const QnUuid kCustomConnectionLocalId;

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

QStandardItem* newConnectionItem(const QString& text, const QUrl& url)
{
    if (url.isEmpty())
        return nullptr;

    auto result = new QStandardItem(text);
    result->setData(url, Qn::UrlRole);
    return result;
}

bool haveToStorePassword(const QnUuid& localId, const QUrl& url)
{
    /**
     * At first we check if we have stored connection to same system
     * with specified user and it stores password.
     */
    const auto recent = qnClientCoreSettings->recentLocalConnections();
    const auto itConnection = std::find_if(recent.begin(), recent.end(),
        [localId, userName = url.userName()](const QnLocalConnectionData& data)
        {
            return ((data.localId == localId) && (userName == data.url.userName()));
        });

    if ((itConnection != recent.end()) && (itConnection->isStoredPassword()))
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

} // anonymous namespace

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
    layout->setContentsMargins(0, 0, 0, 10);
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

    connect(qnModuleFinder, &QnModuleFinder::moduleChanged, this,
        &QnLoginDialog::at_moduleFinder_moduleChanged);
    connect(qnModuleFinder, &QnModuleFinder::moduleAddressFound, this,
        &QnLoginDialog::at_moduleFinder_moduleChanged);
    connect(qnModuleFinder, &QnModuleFinder::moduleLost, this,
        &QnLoginDialog::at_moduleFinder_moduleLost);

    for(const auto& moduleInformation: qnModuleFinder->foundModules())
        at_moduleFinder_moduleChanged(moduleInformation);
}

QnLoginDialog::~QnLoginDialog() {}

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
    m_requestHandle = QnAppServerConnectionFactory::ec2ConnectionFactory()->testConnection(
        url, this,
        [this, url](int handle, ec2::ErrorCode errorCode, const QnConnectionInfo &connectionInfo)
        {
            if (m_requestHandle != handle)
                return; //connect was cancelled

            m_requestHandle = -1;
            updateUsability();

            auto status = QnConnectionDiagnosticsHelper::validateConnection(
                connectionInfo, errorCode, this);

            switch (status)
            {
                case Qn::SuccessConnectionResult:
                {
                    const bool autoLogin = ui->autoLoginCheckBox->isChecked();
                    QnActionParameters params;
                    const bool storePassword =
                        (haveToStorePassword(connectionInfo.localSystemId, url) || autoLogin);
                    params.setArgument(Qn::UrlRole, url);
                    params.setArgument(Qn::AutoLoginRole, autoLogin);
                    params.setArgument(Qn::StorePasswordRole, storePassword);
                    menu()->trigger(QnActions::ConnectAction, params);
                    break;
                }
                case Qn::IncompatibleProtocolConnectionResult:
                    menu()->trigger(QnActions::DelayedForcedExitAction);
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

void QnLoginDialog::resetConnectionsModel()
{
    if (m_lastUsedItem != NULL)
        m_connectionsModel->removeRow(0); //last-used-connection row

    QModelIndex selectedIndex;
    const auto lastConnection = qnSettings->lastUsedConnection();
    m_lastUsedItem = (lastConnection.url.host().isEmpty()
        ? nullptr
        : ::newConnectionItem(tr("* Last used connection *"), lastConnection.url));

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
        url.setUserName(lit("admin"));

        customConnections.append(QnConnectionData(lit("default"), url, kCustomConnectionLocalId));
    }

    m_savedSessionsItem->removeRows(0, m_savedSessionsItem->rowCount());

    std::sort(customConnections.begin(), customConnections.end(),
        [](const QnConnectionData& left, const QnConnectionData& right)
        {
            if (left.isCustom() == right.isCustom())
                return (left.name < right.name);

            return left.isCustom();
        });

    for (const auto& connection : customConnections)
    {
        NX_ASSERT(!connection.name.isEmpty());
        m_savedSessionsItem->appendRow(newConnectionItem(connection));
    }
}

void QnLoginDialog::resetAutoFoundConnectionsModel()
{
    m_autoFoundItem->removeRows(0, m_autoFoundItem->rowCount());
    if (m_foundSystems.size() == 0)
    {
        QStandardItem* noLocalEcs = new QStandardItem(tr("<none>"));
        noLocalEcs->setFlags(Qt::ItemIsEnabled);
        m_autoFoundItem->appendRow(noLocalEcs);
    }
    else
    {
        for (const QnFoundSystemData& data: m_foundSystems)
        {
            QUrl url = data.url;

            auto compatibilityCode = QnConnectionValidator::validateConnection(data.info);

            /* Do not show servers with incompatible customization or cloud host */
            if (!qnRuntime->isDevMode()
                && (compatibilityCode == Qn::IncompatibleInternalConnectionResult
                    || compatibilityCode == Qn::IncompatibleCloudHostConnectionResult))
            {
                    continue;
            }

            bool isCompatible = (compatibilityCode == Qn::SuccessConnectionResult);

            QString title;
            if (!data.info.systemName.isEmpty())
            {
                title = lit("%3 - (%1:%2)")
                    .arg(url.host()).arg(url.port()).arg(data.info.systemName);
            }
            else
            {
                title = lit("%1:%2").arg(url.host()).arg(url.port());
            }

            if (!isCompatible)
            {
                title += lit(" (v%1)")
                    .arg(data.info.version.toString(QnSoftwareVersion::BugfixFormat));
            }

            QStandardItem* item = new QStandardItem(title);
            item->setData(url, Qn::UrlRole);

            if (!isCompatible)
                item->setData(QBrush(qnGlobals->errorTextColor()), Qt::TextColorRole);
            m_autoFoundItem->appendRow(item);
        }
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
        ? lit("admin")  // 99% of users have only one login - admin
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
        const auto button = QnMessageBox::question(
            this,
            tr("Overwrite existing connection?"),
            tr("There is an another connection with the same name."),
            QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
            QDialogButtonBox::Yes);

        switch (button)
        {
            case QDialogButtonBox::Cancel:
                return;
            case QDialogButtonBox::No:
                name = connections.generateUniqueName(name);
                break;
            case QDialogButtonBox::Yes:
                connections.removeOne(name);
                break;
            default:
                break;
        }
    }

    const QUrl url = currentUrl();
    auto connectionData = QnConnectionData(name, url, kCustomConnectionLocalId);

    if (!savePassword)
        connectionData.url.setPassword(QString());
    connections.prepend(connectionData);
    qnSettings->setCustomConnections(connections);

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

    const auto result = QnMessageBox::question(
        this,
        tr("Delete connection?"),
//         tr("Are you sure you want to delete this connection: %1?")
//         + L'\n' + connection.name,
        connection.name,
        QDialogButtonBox::Yes | QDialogButtonBox::No,
        QDialogButtonBox::Yes);

    if (result != QDialogButtonBox::Yes)
        return;

    connections.removeOne(connection.name);
    qnSettings->setCustomConnections(connections);
    resetConnectionsModel();
}

void QnLoginDialog::at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation)
{
    auto addresses = qnModuleFinder->moduleAddresses(moduleInformation.id);

    if (addresses.isEmpty())
    {
        at_moduleFinder_moduleLost(moduleInformation);
        return;
    }

    QnFoundSystemData data;
    data.info = moduleInformation;

    /* prefer localhost */
    SocketAddress address = SocketAddress(QHostAddress(QHostAddress::LocalHost).toString(),
        moduleInformation.port);
    if (!addresses.contains(address))
        address = *addresses.cbegin();

    data.url.setScheme(lit("http"));
    data.url.setHost(address.address.toString());
    data.url.setPort(address.port);

    if (m_foundSystems.contains(moduleInformation.id))
    {
        QnFoundSystemData &oldData = m_foundSystems[moduleInformation.id];
        if (!QHostAddress(address.address.toString()).isLoopback()
                && addresses.contains(oldData.url.host()))
        {
            data.url.setHost(oldData.url.host());
        }

        if (oldData != data)
        {
            oldData = data;
            resetAutoFoundConnectionsModel();
        }
    }
    else
    {
        m_foundSystems.insert(moduleInformation.id, data);
        resetAutoFoundConnectionsModel();
    }
}

void QnLoginDialog::at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation)
{
    if (m_foundSystems.remove(moduleInformation.id))
        resetAutoFoundConnectionsModel();
}
