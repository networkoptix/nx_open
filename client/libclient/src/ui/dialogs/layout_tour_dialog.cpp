#include "layout_tour_dialog.h"
#include "ui_layout_tour_dialog.h"

#include <nx_ec/data/api_layout_tour_data.h>

#include <ui/style/helper.h>
#include <ui/widgets/common/snapped_scrollbar.h>

namespace {

} // namespace


QnLayoutTourDialog::QnLayoutTourDialog(QWidget* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::LayoutTourDialog)
{
     ui->setupUi(this);

     ui->nameInputField->setTitle(tr("Name"));

//
//     QFont font;
//     font.setPixelSize(kLabelFontPixelSize);
//     font.setWeight(kLabelFontWeight);
//     ui->label->setFont(font);
//     ui->label->setProperty(style::Properties::kDontPolishFontProperty, true);
//     ui->label->setForegroundRole(QPalette::Light);
//
     QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(this);
     ui->layoutsTreeView->setVerticalScrollBar(scrollBar->proxyScrollBar());
//
//     m_model = new QnLicenseListModel(this);
//     m_model->setExtendedStatus(true);
//
//     ui->treeView->setModel(m_model);
//     ui->treeView->setItemDelegate(new QnLicenseListItemDelegate(this, false));
//     ui->treeView->setColumnHidden(QnLicenseListModel::ExpirationDateColumn, true);
//     ui->treeView->setColumnHidden(QnLicenseListModel::ServerColumn, true);
//     ui->treeView->setProperty(style::Properties::kSuppressHoverPropery, true);
//
//     ui->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
//
     setResizeToContentsMode(Qt::Horizontal | Qt::Vertical);
}

QnLayoutTourDialog::~QnLayoutTourDialog()
{
}

void QnLayoutTourDialog::loadData(const ec2::ApiLayoutTourData& tour)
{
    ui->nameInputField->setText(tour.name);
}

void QnLayoutTourDialog::submitData(ec2::ApiLayoutTourData* tour) const
{
    NX_ASSERT(tour);
    if (!tour)
        return;

    tour->name = ui->nameInputField->text();
}
