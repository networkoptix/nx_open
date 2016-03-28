#include "login_dialog.h"
#include "ui_login_dialog.h"

#include <QtCore/QEvent>
#include <QtCore/QDir>

#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>

#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QInputDialog>

#include <api/app_server_connection.h>
#include <api/session_manager.h>
#include <api/model/connection_info.h>

#include <client/client_connection_data.h>
#include <client/client_settings.h>

#include <common/common_module.h>

#include <core/resource/resource.h>

#include <ui/actions/action_manager.h>
#include <ui/dialogs/message_box.h>
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

#include <plugins/resource/avi/avi_resource.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <plugins/resource/avi/filetypesupport.h>

#include "connection_testing_dialog.h"

#include "ui/graphics/items/resource/decodedpicturetoopengluploadercontextpool.h"

#include <utils/connection_diagnostics_helper.h>

#include "compatibility.h"
#include <utils/common/app_info.h>
#include <ui/style/custom_style.h>

namespace {
    void setEnabled(const QObjectList &objects, QObject *exclude, bool enabled) {
        foreach(QObject *object, objects)
            if(object != exclude)
                if(QWidget *widget = qobject_cast<QWidget *>(object))
                    widget->setEnabled(enabled);
    }

    QStandardItem *newConnectionItem(QnConnectionData connection) {
        if (connection == QnConnectionData())
            return NULL;
        QStandardItem *result = new QStandardItem(connection.name);
        result->setData(connection.url, Qn::UrlRole);
        return result;
    }

} // anonymous namespace

/************************************************************************/
/* QnEcData                                                             */
/************************************************************************/

QnLoginDialog::QnEcData::QnEcData() :
    protoVersion(0)
{
}

bool QnLoginDialog::QnEcData::operator==(const QnEcData& other) const {
    return
           id == other.id
        && url == other.url
        && version == other.version
        && protoVersion == other.protoVersion
        && systemName == other.systemName
        ;
}

bool QnLoginDialog::QnEcData::operator!=(const QnEcData& other) const {
    return !(*this == other);
}

/************************************************************************/
/* QnLoginDialog                                                        */
/************************************************************************/

QnLoginDialog::QnLoginDialog(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::LoginDialog),
    m_requestHandle(-1),
    m_renderingWidget(NULL)
{
    ui->setupUi(this);

    setAccentStyle(ui->buttonBox->button(QDialogButtonBox::Ok));

    setWindowTitle(tr("Connect to Server..."));
    setHelpTopic(this, Qn::Login_Help);

    QHBoxLayout* bbLayout = dynamic_cast<QHBoxLayout*>(ui->buttonBox->layout());
    NX_ASSERT(bbLayout);
    if (bbLayout) {
        QLabel* versionLabel = new QLabel(ui->buttonBox);
        versionLabel->setText(tr("Version %1").arg(QnAppInfo::applicationVersion()));
        QFont font = versionLabel->font();
        font.setPointSize(7);
        versionLabel->setFont(font);
        bbLayout->insertWidget(0, versionLabel);
    }

    static const char *introNames[] = { "intro.mkv", "intro.avi", "intro.png", "intro.jpg", "intro.jpeg", NULL };
    QString introPath;
    for(const char **introName = introNames; *introName != NULL; introName++) {
        introPath = qnSkin->path(*introName);
        if(!introPath.isEmpty())
            break;
    }

    QnAviResourcePtr resource = QnAviResourcePtr(new QnAviResource(lit("qtfile://") + introPath));
    if (FileTypeSupport::isImageFileExt(introPath))
        resource->addFlags(Qn::still_image);

    m_renderingWidget = QnGlWidgetFactory::create<QnRenderingWidget>();
    m_renderingWidget->setResource(resource);

    QVBoxLayout* layout = new QVBoxLayout(ui->videoSpacer);
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,10);
    layout->addWidget(m_renderingWidget);
    DecodedPictureToOpenGLUploaderContextPool::instance()->ensureThereAreContextsSharedWith( m_renderingWidget );

    m_connectionsModel = new QStandardItemModel(this);
    ui->connectionsComboBox->setModel(m_connectionsModel);

    m_lastUsedItem = NULL;
    m_savedSessionsItem = new QStandardItem(tr("Saved Sessions"));
    m_savedSessionsItem->setFlags(Qt::ItemIsEnabled);
    m_autoFoundItem = new QStandardItem(tr("Auto-Discovered Servers"));
    m_autoFoundItem->setFlags(Qt::ItemIsEnabled);

    m_connectionsModel->appendRow(m_savedSessionsItem);
    m_connectionsModel->appendRow(m_autoFoundItem);

    connect(ui->connectionsComboBox,        SIGNAL(currentIndexChanged(QModelIndex)), this,   SLOT(at_connectionsComboBox_currentIndexChanged(QModelIndex)));
    connect(ui->testButton,                 SIGNAL(clicked()),                      this,   SLOT(at_testButton_clicked()));
    connect(ui->saveButton,                 SIGNAL(clicked()),                      this,   SLOT(at_saveButton_clicked()));
    connect(ui->deleteButton,               SIGNAL(clicked()),                      this,   SLOT(at_deleteButton_clicked()));
    connect(ui->passwordLineEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->loginLineEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->hostnameLineEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->portSpinBox,                SIGNAL(valueChanged(int)),              this,   SLOT(updateAcceptibility()));
    connect(ui->buttonBox,                  SIGNAL(accepted()),                     this,   SLOT(accept()));
    connect(ui->buttonBox,                  SIGNAL(rejected()),                     this,   SLOT(reject()));

    resetConnectionsModel();
    updateFocus();

    /* Should be done after model resetting to avoid state loss. */
    ui->autoLoginCheckBox->setChecked(qnSettings->autoLogin());
    connect(ui->autoLoginCheckBox, &QCheckBox::stateChanged,    this, [this](int state) {
        qnSettings->setAutoLogin(state == Qt::Checked);
    });

    connect(QnModuleFinder::instance(), &QnModuleFinder::moduleChanged,         this, &QnLoginDialog::at_moduleFinder_moduleChanged);
    connect(QnModuleFinder::instance(), &QnModuleFinder::moduleAddressFound,    this, &QnLoginDialog::at_moduleFinder_moduleChanged);
    connect(QnModuleFinder::instance(), &QnModuleFinder::moduleLost,            this, &QnLoginDialog::at_moduleFinder_moduleLost);

    foreach (const QnModuleInformation &moduleInformation, QnModuleFinder::instance()->foundModules())
        at_moduleFinder_moduleChanged(moduleInformation);
}

QnLoginDialog::~QnLoginDialog() {}

void QnLoginDialog::updateFocus() {
    ui->passwordLineEdit->setFocus();
    ui->passwordLineEdit->selectAll();
}

QUrl QnLoginDialog::currentUrl() const {
    QUrl url;
    url.setScheme(lit("http"));
    url.setHost(ui->hostnameLineEdit->text().trimmed());
    url.setPort(ui->portSpinBox->value());
    url.setUserName(ui->loginLineEdit->text().trimmed());
    url.setPassword(ui->passwordLineEdit->text());
    return url;
}

void QnLoginDialog::accept() {
    QUrl url = currentUrl();
    if (!url.isValid()) {
        QnMessageBox::warning(this, tr("Invalid Login Information"), tr("The login information you have entered is not valid."));
        return;
    }

    QString name = ui->connectionsComboBox->currentText();

    m_requestHandle = QnAppServerConnectionFactory::ec2ConnectionFactory()->testConnection(
        url,
        this,
        [this, url, name](int handle, ec2::ErrorCode errorCode, const QnConnectionInfo &connectionInfo)
    {
        if (m_requestHandle != handle)
            return; //connect was cancelled

        m_requestHandle = -1;
        updateUsability();

        QnConnectionDiagnosticsHelper::Result status = QnConnectionDiagnosticsHelper::validateConnection(connectionInfo, errorCode, url, this);
        switch (status) {
        case QnConnectionDiagnosticsHelper::Result::Success:
            menu()->trigger(QnActions::ConnectAction, QnActionParameters().withArgument(Qn::UrlRole, url));
            updateStoredConnections(url, name);
            break;
        case QnConnectionDiagnosticsHelper::Result::RestartRequested:
            menu()->trigger(QnActions::DelayedForcedExitAction);
            break; // to avoid cycle
        default:    //error
            return;
        }

        base_type::accept();
    });
    updateUsability();
}

void QnLoginDialog::reject() {
    if(m_requestHandle == -1) {
        base_type::reject();
        return;
    }

    m_requestHandle = -1;
    updateUsability();
}

void QnLoginDialog::changeEvent(QEvent *event) {
    base_type::changeEvent(event);

    switch (event->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void QnLoginDialog::showEvent(QShowEvent *event) {
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

void QnLoginDialog::resetConnectionsModel() {
    QnConnectionDataList connections = qnSettings->customConnections();

    if (m_lastUsedItem != NULL)
        m_connectionsModel->removeRow(0); //last-used-connection row

    QModelIndex selectedIndex;
    m_lastUsedItem = newConnectionItem(connections.getByName(QnConnectionDataList::defaultLastUsedNameKey()));
    if (m_lastUsedItem != NULL) {
        m_lastUsedItem->setText(tr("* Last used connection *"));
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

void QnLoginDialog::resetSavedSessionsModel() {
    QnConnectionDataList connections = qnSettings->customConnections();
    if (connections.isEmpty())
        connections.append(qnSettings->defaultConnection());

    m_savedSessionsItem->removeRows(0, m_savedSessionsItem->rowCount());
    foreach (const QnConnectionData &connection, connections) {
        if (connection.name == QnConnectionDataList::defaultLastUsedNameKey())
            continue;
        m_savedSessionsItem->appendRow(newConnectionItem(connection));
    }
}

void QnLoginDialog::resetAutoFoundConnectionsModel() {
    m_autoFoundItem->removeRows(0, m_autoFoundItem->rowCount());
    if (m_foundEcs.size() == 0) {
        QStandardItem* noLocalEcs = new QStandardItem(tr("<none>"));
        noLocalEcs->setFlags(Qt::ItemIsEnabled);
        m_autoFoundItem->appendRow(noLocalEcs);
    } else {
        foreach (const QnEcData& data, m_foundEcs) {
            QUrl url = data.url;

            auto compatibilityCode = QnConnectionDiagnosticsHelper::validateConnectionLight(QString(), data.version, data.protoVersion);

            bool isCompatible = (compatibilityCode == QnConnectionDiagnosticsHelper::Result::Success);

            QString title;
            if (!data.systemName.isEmpty())
                title = lit("%3 - (%1:%2)").arg(url.host()).arg(url.port()).arg(data.systemName);
            else
                title = lit("%1:%2").arg(url.host()).arg(url.port());
            if (!isCompatible)
                title += lit(" (v%1)").arg(data.version.toString(QnSoftwareVersion::BugfixFormat));
#ifdef _DEBUG
            if (!isCompatible)
                title += lit(" (%1)").arg(QnConnectionDiagnosticsHelper::resultToString(compatibilityCode));
#endif // _DEBUG


            QStandardItem* item = new QStandardItem(title);
            item->setData(url, Qn::UrlRole);

            if (!isCompatible)
                item->setData(QColor(Qt::red), Qt::TextColorRole);
            m_autoFoundItem->appendRow(item);
        }
    }
}

void QnLoginDialog::updateAcceptibility() {
    bool acceptable =
        !ui->passwordLineEdit->text().isEmpty() &&
        !ui->loginLineEdit->text().trimmed().isEmpty() &&
        !ui->hostnameLineEdit->text().trimmed().isEmpty() &&
        ui->portSpinBox->value() != 0;

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(acceptable);
    ui->testButton->setEnabled(acceptable);
}

void QnLoginDialog::updateUsability() {
    if(m_requestHandle == -1) {
        ::setEnabled(children(), ui->buttonBox, true);
        ::setEnabled(ui->buttonBox->children(), ui->buttonBox->button(QDialogButtonBox::Cancel), true);
        unsetCursor();
        ui->buttonBox->button(QDialogButtonBox::Cancel)->unsetCursor();

        updateAcceptibility();
        updateFocus();
    } else {
        ::setEnabled(children(), ui->buttonBox, false);
        ::setEnabled(ui->buttonBox->children(), ui->buttonBox->button(QDialogButtonBox::Cancel), false);
        setCursor(Qt::BusyCursor);
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setCursor(Qt::ArrowCursor);
    }
}

void QnLoginDialog::updateStoredConnections(const QUrl &url, const QString &name) {
    QnConnectionDataList connections = qnSettings->customConnections();

    QUrl urlToSave(url);
    if (!ui->autoLoginCheckBox->isChecked())
        urlToSave.setPassword(QString());

    QnConnectionData connectionData(name, urlToSave);
    qnSettings->setLastUsedConnection(connectionData);

    // remove previous "Last used connection"
    connections.removeOne(QnConnectionDataList::defaultLastUsedNameKey());

    QnConnectionData selected = connections.getByName(name);
    if (qnUrlEqual(selected.url, url)) {
        connections.removeOne(selected.name);
        connections.prepend(selected);    /* Reorder. */
    } else {
        // save "Last used connection"
        QnConnectionData last(connectionData);
        last.name = QnConnectionDataList::defaultLastUsedNameKey();
        connections.prepend(last);
    }
    qnSettings->setCustomConnections(connections);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnLoginDialog::at_connectionsComboBox_currentIndexChanged(const QModelIndex &index) {
    QStandardItem* item = m_connectionsModel->itemFromIndex(index);
    QUrl url = item == NULL ? QUrl() : item->data(Qn::UrlRole).toUrl();
    ui->hostnameLineEdit->setText(url.host());
    ui->portSpinBox->setValue(url.port());
    ui->loginLineEdit->setText(url.userName().isEmpty()
        ? lit("admin")  // 99% of users have only one login - admin
        : url.userName());
    ui->passwordLineEdit->setText(url.password());
    ui->deleteButton->setEnabled(qnSettings->customConnections().contains(ui->connectionsComboBox->currentText()));
    ui->autoLoginCheckBox->setChecked(false);
    updateFocus();
}

void QnLoginDialog::at_testButton_clicked() {
    QUrl url = currentUrl();

    if (!url.isValid()) {
        QnMessageBox::warning(this, tr("Invalid Parameters"), tr("The information you have entered is not valid."));
        return;
    }

    bool connectRequested = false;

    QScopedPointer<QnConnectionTestingDialog> dialog(new QnConnectionTestingDialog(this));
    connect(dialog.data(), &QnConnectionTestingDialog::connectRequested, this, [&connectRequested] {
        connectRequested = true;
    });

    dialog->testConnection(url);
    dialog->exec();

    updateFocus();
    if (connectRequested)
        accept();
}

void QnLoginDialog::at_saveButton_clicked() {
    QUrl url = currentUrl();

    if (!url.isValid()) {
        QnMessageBox::warning(this, tr("Invalid Paramaters"), tr("Entered hostname is not valid."));
        ui->hostnameLineEdit->setFocus();
        return;
    }

    if (url.host().length() == 0) {
        QnMessageBox::warning(this, tr("Invalid Paramaters"), tr("Host field cannot be empty."));
        ui->hostnameLineEdit->setFocus();
        return;
    }

    QnConnectionDataList connections = qnSettings->customConnections();

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


    // save here because of the 'connections' field modifying
    QString password = ui->passwordLineEdit->text();
    bool autoLogin = qnSettings->autoLogin();

    if (connections.contains(name)){
        QDialogButtonBox::StandardButton button = QnMessageBox::warning(this, tr("Connection already exists."),
                                                                  tr("A connection with this name already exists. Do you want to overwrite it?"),
                                                                  QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
                                                                  QDialogButtonBox::Yes);
        switch(button) {
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

    QnConnectionData connectionData(name, currentUrl());
    if (!savePassword)
        connectionData.url.setPassword(QString());
    connections.prepend(connectionData);
    qnSettings->setCustomConnections(connections);

    resetSavedSessionsModel();

    QModelIndex idx = m_connectionsModel->index(0, 0, m_savedSessionsItem->index());
    ui->connectionsComboBox->setCurrentIndex(idx);
    at_connectionsComboBox_currentIndexChanged(idx); // call directly in case index change will not work
    ui->passwordLineEdit->setText(password);         // password is cleared on index change
    ui->autoLoginCheckBox->setChecked(autoLogin);

}

void QnLoginDialog::at_deleteButton_clicked() {
    QnConnectionDataList connections = qnSettings->customConnections();

    QString name = ui->connectionsComboBox->currentText();
    if (!connections.contains(name))
        return;

    if (QnMessageBox::warning(this, tr("Delete Connections"),
                                   tr("Are you sure you want to delete this connection: %1?").arg(L'\n' + name),
                             QDialogButtonBox::Yes, QDialogButtonBox::No) == QDialogButtonBox::No)
        return;


    connections.removeOne(name);
    qnSettings->setCustomConnections(connections);
    resetConnectionsModel();
}

void QnLoginDialog::at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation) {
    QSet<SocketAddress> addresses = QnModuleFinder::instance()->moduleAddresses(moduleInformation.id);

    if (addresses.isEmpty()) {
        at_moduleFinder_moduleLost(moduleInformation);
        return;
    }

    QnEcData data;
    data.id = moduleInformation.id;
    data.version = moduleInformation.version;
    data.protoVersion = moduleInformation.protoVersion;
    data.systemName = moduleInformation.systemName;

    /* prefer localhost */
    SocketAddress address = SocketAddress(QHostAddress(QHostAddress::LocalHost).toString(), moduleInformation.port);
    if (!addresses.contains(address))
        address = *addresses.cbegin();

    data.url.setScheme(lit("http"));
    data.url.setHost(address.address.toString());
    data.url.setPort(address.port);

    if (m_foundEcs.contains(moduleInformation.id)) {
        QnEcData &oldData = m_foundEcs[moduleInformation.id];
        if (!QHostAddress(address.address.toString()).isLoopback() && addresses.contains(oldData.url.host()))
            data.url.setHost(oldData.url.host());

        if (oldData != data) {
            oldData = data;
            resetAutoFoundConnectionsModel();
        }
    } else {
        m_foundEcs.insert(moduleInformation.id, data);
        resetAutoFoundConnectionsModel();
    }
}

void QnLoginDialog::at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation) {
    if (m_foundEcs.remove(moduleInformation.id))
        resetAutoFoundConnectionsModel();
}
