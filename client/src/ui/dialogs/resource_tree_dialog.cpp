#include "resource_tree_dialog.h"
#include "ui_resource_tree_dialog.h"

#include <ui/models/resource_pool_model.h>
#include <ui/workbench/workbench_context.h>

QnResourceTreeDialog::QnResourceTreeDialog(QWidget *parent, QnWorkbenchContext *context) :
    QDialog(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::QnResourceTreeDialog())
{
    ui->setupUi(this);

    m_resourceModel = new QnResourcePoolModel(this, Qn::ServersNode);
    ui->resourcesWidget->setModel(m_resourceModel); //TODO: move proxy model out of browser class
}

QnResourceTreeDialog::~QnResourceTreeDialog()
{

}
