#include "login_dialog.h"
#include "ui_login_dialog.h"

#include <QEvent>
#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "ui/workbench/workbench_context.h"
#include "connection_testing_dialog.h"

#include "api/AppServerConnection.h"
#include "api/SessionManager.h"

#include "utils/settings.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "camera/camera.h"
#include "camera/gl_renderer.h"
#include "ui/graphics/items/resource_widget_renderer.h"
#include "plugins/resources/archive/filetypes.h"
#include "plugins/resources/archive/filetypesupport.h"

namespace {
    void setEnabled(const QObjectList &objects, QObject *exclude, bool enabled) {
        foreach(QObject *object, objects)
            if(object != exclude)
                if(QWidget *widget = qobject_cast<QWidget *>(object))
                    widget->setEnabled(enabled);
    }

} // anonymous namespace

class QnMyGLWidget: public QGLWidget
{
public:
    QnMyGLWidget(const QGLFormat &format, QWidget *parent = 0): QGLWidget(format, parent)
    {
        m_renderer = 0;
        connect(&m_timer, SIGNAL(timeout()), this, SLOT(update()));
        m_timer.start(16);
        m_firstTime = true;
    }

    virtual void resizeEvent(QResizeEvent *event) override
    {
        static double sar = 1.0;
        double windowHeight = event->size().height();
        double windowWidth = event->size().width();
        double textureWidth = 1920;
        double textureHeight = 1080;
        double newTextureWidth = static_cast<uint>(textureWidth * sar);

        double windowAspect = windowWidth / windowHeight;
        double textureAspect = textureWidth / textureHeight;
        if (windowAspect > textureAspect)
        {
            // black bars at left and right
            m_videoRect.setTop(0);
            m_videoRect.setHeight(windowHeight);
            double scale = windowHeight / textureHeight;
            double scaledWidth = newTextureWidth * scale;
            m_videoRect.setLeft((windowWidth - scaledWidth) / 2);
            m_videoRect.setWidth(scaledWidth + 0.5);
        }
        else 
        {
            // black bars at the top and bottom
            m_videoRect.setLeft(0);
            m_videoRect.setWidth(windowWidth);
            if (newTextureWidth < windowWidth) {
                double scale = windowWidth / newTextureWidth;
                double scaledHeight = textureHeight * scale;
                m_videoRect.setTop((windowHeight - scaledHeight) / 2);
                m_videoRect.setHeight(scaledHeight + 0.5);
            }
            else {
                double newTextureHeight = windowWidth / textureAspect + 0.5;
                m_videoRect.setTop((windowHeight - newTextureHeight) / 2);
                m_videoRect.setHeight(newTextureHeight);
            }
        }

        
        if (m_renderer)
            m_renderer->setChannelScreenSize(m_videoRect.size());
    }

    void setRenderer(QnResourceWidgetRenderer *renderer)
    {
        m_renderer = renderer;
    }

    
    virtual void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.beginNativePainting();
        if (m_renderer)
            m_renderer->paint(0, m_videoRect, 1.0);
        painter.endNativePainting();
        if (m_firstTime)
        {
            if (m_camDisplay)
                m_camDisplay->start();
            m_firstTime = false;
        }
    }

    void setCamDisplay(CLCamDisplay* camDisplay)
    {
        m_camDisplay = camDisplay;
    }
    
private:
    QRect m_videoRect;
    QnResourceWidgetRenderer *m_renderer;
    QTimer m_timer;
    CLCamDisplay *m_camDisplay;
    bool m_firstTime;
};

// ------------------------------------

LoginDialog::LoginDialog(QnWorkbenchContext *context, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog),
    m_context(context),
    m_requestHandle(-1)
{
    dataProvider = 0;
    camera = 0;
    renderer = 0;
    glWindow = 0;


    if(!context)
        qnNullWarning(context);

    ui->setupUi(this);
    QPushButton *resetButton = ui->buttonBox->button(QDialogButtonBox::Reset);

    /* Don't allow to save passwords, at least for now. */
    //ui->savePasswordCheckBox->hide();

    //ui->titleLabel->setAlignment(Qt::AlignCenter);
    //ui->titleLabel->setPixmap(qnSkin->pixmap("logo_1920_1080.png", QSize(250, 1000), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QVBoxLayout* layout = new QVBoxLayout(ui->videoSpacer);
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,10);

    QGLFormat glFormat;
    glFormat.setOption(QGL::SampleBuffers); /* Multisampling. */
    glFormat.setSwapInterval(1); /* Turn vsync on. */
    glWindow = new QnMyGLWidget(glFormat);
    layout->addWidget(glWindow);

    QDir dir(":/skin");
    QStringList	introList = dir.entryList(QStringList() << "intro.*");
    QString resourceName = ":/skin/intro";
    if (!introList.isEmpty())
        resourceName = QString(":/skin/") + introList.first();

    aviRes = QnAviResourcePtr(new QnAviResource(QString("qtfile://") + resourceName));
    if (FileTypeSupport::isImageFileExt(resourceName))
        aviRes->addFlags(QnResource::still_image);

    dataProvider = static_cast<QnAbstractArchiveReader*>(aviRes->createDataProvider(QnResource::Role_Default));
    dataProvider->setCycleMode(false);
    camera = new CLVideoCamera(aviRes, false, dataProvider);
    
    renderer = new QnResourceWidgetRenderer(1, 0, glWindow->context());
    glWindow->setRenderer(renderer);
    camera->getCamDisplay()->addVideoChannel(0, renderer, true);
    camera->getCamDisplay()->setMTDecoding(true);
    dataProvider->start();

    glWindow->setCamDisplay(camera->getCamDisplay());



    resetButton->setText(tr("&Reset"));

    m_connectionsModel = new QStandardItemModel(this);
    ui->connectionsComboBox->setModel(m_connectionsModel);

    connect(ui->connectionsComboBox,        SIGNAL(currentIndexChanged(int)),       this,   SLOT(at_connectionsComboBox_currentIndexChanged(int)));
    connect(ui->testButton,                 SIGNAL(clicked()),                      this,   SLOT(at_testButton_clicked()));
    connect(ui->configureConnectionsButton, SIGNAL(clicked()),                      this,   SLOT(at_configureConnectionsButton_clicked()));
    connect(ui->passwordLineEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->loginLineEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->hostnameLineEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->portSpinBox,                SIGNAL(valueChanged(int)),              this,   SLOT(updateAcceptibility()));
    connect(ui->buttonBox,                  SIGNAL(accepted()),                     this,   SLOT(accept()));
    connect(ui->buttonBox,                  SIGNAL(rejected()),                     this,   SLOT(reject()));
    connect(resetButton,                    SIGNAL(clicked()),                      this,   SLOT(reset()));

    m_dataWidgetMapper = new QDataWidgetMapper(this);
    m_dataWidgetMapper->setModel(m_connectionsModel);
    m_dataWidgetMapper->setSubmitPolicy(QDataWidgetMapper::AutoSubmit);
    m_dataWidgetMapper->setOrientation(Qt::Horizontal);
    m_dataWidgetMapper->addMapping(ui->hostnameLineEdit, 1);
    m_dataWidgetMapper->addMapping(ui->portSpinBox, 2);
    m_dataWidgetMapper->addMapping(ui->loginLineEdit, 3);
    m_dataWidgetMapper->addMapping(ui->passwordLineEdit, 4);

    updateStoredConnections();
    updateFocus();
}

LoginDialog::~LoginDialog()
{
    renderer->beforeDestroy();
    camera->beforeStopDisplay();
    camera->stopDisplay();
    
    delete camera;
    delete renderer;
    delete glWindow;
}

void LoginDialog::updateFocus() 
{
    int size = m_dataWidgetMapper->model()->columnCount();

    int i;
    for(i = 0; i < size; i++) {
        QWidget *widget = m_dataWidgetMapper->mappedWidgetAt(i);
        if(!widget)
            continue;

        QByteArray propertyName = m_dataWidgetMapper->mappedPropertyName(widget);
        QVariant value = widget->property(propertyName.constData());
        if(!value.isValid())
            continue;

        if(value.toString().isEmpty())
            break;

        if((value.userType() == QVariant::Int || value.userType() == QVariant::LongLong) && value.toInt() == 0)
            break;
    }

    if(i < size)
        m_dataWidgetMapper->mappedWidgetAt(i)->setFocus();
}

QUrl LoginDialog::currentUrl()
{
    const int row = ui->connectionsComboBox->currentIndex();

    QUrl url;

    QString host = m_connectionsModel->item(row, 1)->text();
    int port = m_connectionsModel->item(row, 2)->text().toInt();

    url.setScheme("http");
    url.setHost(host);
    url.setPort(port);
    url.setUserName(m_connectionsModel->item(row, 3)->text());
    url.setPassword(m_connectionsModel->item(row, 4)->text());

    return url;
}

void LoginDialog::accept()
{
    /* Widget data may not be written out to the model yet. Force it. */
    m_dataWidgetMapper->submit();

    QUrl url = currentUrl();
    if (!url.isValid()) {
        QMessageBox::warning(this, tr("Invalid Login Information"), tr("The Login Information you have entered is not valid."));
        return;
    }

    QnAppServerConnectionPtr connection = QnAppServerConnectionFactory::createConnection(url);
    m_requestHandle = connection->testConnectionAsync(this, SLOT(at_testFinished(int, const QByteArray &, const QByteArray &, int)));

    updateUsability();
}

void LoginDialog::reject() 
{
    if(m_requestHandle == -1) {
        QDialog::reject();
        return;
    }

    m_requestHandle = -1;
    updateUsability();
}

void LoginDialog::reset()
{
    ui->hostnameLineEdit->clear();
    ui->portSpinBox->setValue(0);
    ui->loginLineEdit->clear();
    ui->passwordLineEdit->clear();

    updateStoredConnections();
}

void LoginDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);

    switch (event->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void LoginDialog::updateStoredConnections()
{
    m_connectionsModel->removeRows(0, m_connectionsModel->rowCount());

    int index = -1;
    QnSettings::ConnectionData lastUsedConnection = qnSettings->lastUsedConnection();
    foreach (const QnSettings::ConnectionData &connection, qnSettings->connections()) {
        if (connection.name.trimmed().isEmpty() || !connection.url.isValid())
            continue;

        QList<QStandardItem *> row;
        row << new QStandardItem(connection.name)
            << new QStandardItem(connection.url.host())
            << new QStandardItem(QString::number(connection.url.port()))
            << new QStandardItem(connection.url.userName())
            << new QStandardItem(connection.url.password());
        m_connectionsModel->appendRow(row);

        if (lastUsedConnection == connection)
            index = m_connectionsModel->rowCount() - 1;
    }
    {
        QnSettings::ConnectionData connection;
        if (index == -1 || lastUsedConnection.name.trimmed().isEmpty()) {
            connection = lastUsedConnection;
            if (index == -1) {
                index = 0;
                connection.name.clear();
            }
        }
        QList<QStandardItem *> row;
        row << new QStandardItem(connection.name)
            << new QStandardItem(connection.url.host())
            << new QStandardItem(QString::number(connection.url.port()))
            << new QStandardItem(connection.url.userName())
            << new QStandardItem(connection.url.password());
        m_connectionsModel->insertRow(0, row);
    }

    ui->connectionsComboBox->setCurrentIndex(index);
}

void LoginDialog::updateAcceptibility() 
{
    bool acceptable = 
        !ui->passwordLineEdit->text().isEmpty() &&
        !ui->loginLineEdit->text().trimmed().isEmpty() &&
        !ui->hostnameLineEdit->text().trimmed().isEmpty() &&
        ui->portSpinBox->value() != 0;

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(acceptable);
    ui->testButton->setEnabled(acceptable);
}

void LoginDialog::updateUsability() {
    if(m_requestHandle == -1) {
        ::setEnabled(children(), ui->buttonBox, true);
        ::setEnabled(ui->buttonBox->children(), ui->buttonBox->button(QDialogButtonBox::Cancel), true);
        unsetCursor();
        ui->buttonBox->button(QDialogButtonBox::Cancel)->unsetCursor();

        updateAcceptibility();
    } else {
        ::setEnabled(children(), ui->buttonBox, false);
        ::setEnabled(ui->buttonBox->children(), ui->buttonBox->button(QDialogButtonBox::Cancel), false);
        setCursor(Qt::BusyCursor);
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setCursor(Qt::ArrowCursor);
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void LoginDialog::at_testFinished(int status, const QByteArray &/*data*/, const QByteArray &/*errorString*/, int requestHandle)
{
    if(m_requestHandle != requestHandle) 
        return;
    m_requestHandle = -1;

    updateUsability();

    if(status != 0) {
        QMessageBox::warning(
            this, 
            tr("Could not connect to Enterprise Controller"), 
            tr("Connection to the Enterprise Controller could not be established.\nConnection details that you have entered are incorrect, please try again.\n\nIf this error persists, please contact your VMS administrator.")
        );
        return;
    }

    QnSettings::ConnectionData connectionData;
    connectionData.url = currentUrl();
    qnSettings->setLastUsedConnection(connectionData);

    QDialog::accept();
}

void LoginDialog::at_connectionsComboBox_currentIndexChanged(int index)
{
    m_dataWidgetMapper->setCurrentModelIndex(m_connectionsModel->index(index, 0));
}

void LoginDialog::at_testButton_clicked()
{
    QUrl url = currentUrl();

    if (!url.isValid())
    {
        QMessageBox::warning(this, tr("Invalid paramters"), tr("The information you have entered is not valid."));
        return;
    }

    ConnectionTestingDialog dialog(this, url);
    dialog.setModal(true);
    dialog.exec();
}

void LoginDialog::at_configureConnectionsButton_clicked()
{
    QScopedPointer<QnPreferencesDialog> dialog(new QnPreferencesDialog(m_context.data(), this));
    dialog->setCurrentPage(QnPreferencesDialog::PageConnections);

    if (dialog->exec() == QDialog::Accepted)
        updateStoredConnections();
}
