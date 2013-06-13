#include "camera_list_dialog.h"

#include <QClipboard>
#include <QMenu>

#include "ui_camera_list_dialog.h"
#include "ui/models/camera_list_model.h"
#include "ui/workbench/workbench_context.h"
#include "core/resource_managment/resource_pool.h"
#include "ui/models/resource_search_proxy_model.h"
#include "ui/actions/action_manager.h"
#include "ui/workbench/workbench.h"
#include "core/resource/camera_resource.h"

QnCameraListDialog::QnCameraListDialog(QWidget *parent, QnWorkbenchContext *context):
    QDialog(parent),
    ui(new Ui::CameraListDialog),
    m_context(context)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    m_model = new QnCameraListModel(context, this);
    m_model->setResources(context->resourcePool()->getAllEnabledCameras());
    connect(context->resourcePool(),  SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resPool_resourceRemoved(const QnResourcePtr &)));
    connect(context->resourcePool(),  SIGNAL(resourceAdded(const QnResourcePtr &)), this,   SLOT(at_resPool_resourceAdded(const QnResourcePtr &)));

    QList<QnCameraListModel::Column> columns;
    columns << QnCameraListModel::RecordingColumn << QnCameraListModel::NameColumn << QnCameraListModel::VendorColumn << QnCameraListModel::ModelColumn <<
               QnCameraListModel::FirmwareColumn << QnCameraListModel::IPColumn << QnCameraListModel::UniqIdColumn << QnCameraListModel::ServerColumn;

    m_model->setColumns(columns);

    m_resourceSearch = new QnResourceSearchProxyModel(this);
    connect(m_resourceSearch,  SIGNAL(criteriaChanged()), this, SLOT(at_modelChanged()) );
    connect(m_resourceSearch,  SIGNAL(modelReset()), this, SLOT(at_modelChanged()) );
    m_resourceSearch->setSourceModel(m_model);
    m_resourceSearch->addCriterion(QnResourceCriterion(QRegExp(lit("*"),Qt::CaseInsensitive, QRegExp::Wildcard)));
    
    connect(ui->SearchString, SIGNAL(textChanged(const QString&)), this, SLOT(at_searchStringChanged(const QString&)));
    connect(ui->gridCameras,  SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(at_customContextMenuRequested(const QPoint&)) );
    connect(ui->gridCameras,  SIGNAL(doubleClicked(const QModelIndex& )), this, SLOT(at_gridDoublelClicked(const QModelIndex&)) );

    ui->gridCameras->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->gridCameras->setModel(m_resourceSearch);

    m_clipboardAction   = new QAction(tr("Copy selection to clipboard"), this);
    connect(m_clipboardAction,      SIGNAL(triggered()),                this, SLOT(at_copyToClipboard()));
    m_clipboardAction->setShortcut(QKeySequence::Copy);
    ui->gridCameras->addAction(m_clipboardAction);

    ui->gridCameras->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
}

QnCameraListDialog::~QnCameraListDialog()
{
}

void QnCameraListDialog::at_searchStringChanged(const QString& text)
{
    QString searchString = QString(lit("*%1*")).arg(text);
    m_resourceSearch->clearCriteria();
    m_resourceSearch->addCriterion(QnResourceCriterion(QRegExp(searchString, Qt::CaseInsensitive, QRegExp::Wildcard)));
}

void QnCameraListDialog::at_gridDoublelClicked(const QModelIndex& idx)
{
    if (idx.isValid()) 
    {
        QnResourcePtr resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (resource)
            m_context->menu()->trigger(Qn::OpenInCameraSettingsDialogAction, QnActionParameters(resource));
    }
}

void QnCameraListDialog::at_customContextMenuRequested(const QPoint&)
{
    QModelIndexList list = ui->gridCameras->selectionModel()->selectedRows();
    QnResourceList resList;
    foreach(QModelIndex idx, list)
    {
        QnResourcePtr resource = idx.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (resource)
            resList << resource;
    }

    QMenu* menu = 0;
    QnActionManager* manager = m_context->menu();

    if (!resList.isEmpty()) {
        menu = manager->newMenu(Qn::TreeScope, QnActionParameters(resList));
        foreach(QAction* action, menu->actions())
            action->setShortcut(QKeySequence());
    }

    if (menu)
        menu->addSeparator();
    else
        menu = new QMenu();

    m_clipboardAction->setEnabled(ui->gridCameras->selectionModel()->hasSelection());
    menu->addAction(m_clipboardAction);

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void QnCameraListDialog::at_copyToClipboard()
{
    QAbstractItemModel *model = ui->gridCameras->model();
    QModelIndexList list = ui->gridCameras->selectionModel()->selectedIndexes();
    if(list.isEmpty())
        return;

    qSort(list);

    QString textData;
    QString htmlData;
    QMimeData* mimeData = new QMimeData();

    htmlData.append(lit("<html>\n"));
    htmlData.append(lit("<body>\n"));
    htmlData.append(lit("<table>\n"));

    htmlData.append(lit("<tr>"));
    for(int i = 0; i < list.size() && list[i].row() == list[0].row(); ++i)
    {
        if (i > 0)
            textData.append(lit('\t'));
        QString header = model->headerData(list[i].column(), Qt::Horizontal).toString();
        htmlData.append(lit("<th>"));
        htmlData.append(header);
        htmlData.append(lit("</th>"));
        textData.append(header);
    }
    htmlData.append(lit("</tr>"));

    int prevRow = -1;
    for(int i = 0; i < list.size(); ++i)
    {
        if(list[i].row() != prevRow) {
            prevRow = list[i].row();
            textData.append(lit('\n'));
            if (i > 0)
                htmlData.append(lit("</tr>"));
            htmlData.append(lit("<tr>"));
        }
        else {
            textData.append(lit('\t'));
        }

        htmlData.append(lit("<td>"));
        htmlData.append(model->data(list[i], Qt::DisplayRole).toString());
        htmlData.append(lit("</td>"));

        textData.append(model->data(list[i]).toString());
    }
    htmlData.append(lit("</tr>\n"));
    htmlData.append(lit("</table>\n"));
    htmlData.append(lit("</body>\n"));
    htmlData.append(lit("</html>\n"));
    textData.append(lit('\n'));

    mimeData->setText(textData);
    mimeData->setHtml(htmlData);

    QApplication::clipboard()->setMimeData(mimeData);
}

void QnCameraListDialog::at_modelChanged()
{
    setWindowTitle(QString(lit("Cameras list - %1 camera(s) found")).arg(m_resourceSearch->rowCount()));
}

void QnCameraListDialog::at_resPool_resourceRemoved(const QnResourcePtr & resource)
{
    m_model->removeResource(resource);
}

void QnCameraListDialog::at_resPool_resourceAdded(const QnResourcePtr & resource)
{
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (camera)
        m_model->addResource(camera);
}
