#include "resource_tree_dialog.h"
#include "ui_resource_tree_dialog.h"

#include <ui/models/resource_pool_model.h>

QnResourceTreeDialog::QnResourceTreeDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnResourceTreeDialog())
{
    ui->setupUi(this);

    m_resourceModel = new QnResourcePoolModel();

    ui->resourcesWidget->setModel(m_resourceModel); //TODO: move proxy model out of browser class
}

QnResourceTreeDialog::~QnResourceTreeDialog()
{

}
