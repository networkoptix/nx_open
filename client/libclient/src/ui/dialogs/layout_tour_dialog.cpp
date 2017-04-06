#include "layout_tour_dialog.h"
#include "ui_layout_tour_dialog.h"

#include <ui/style/helper.h>
#include <ui/widgets/common/snapped_scrollbar.h>

namespace {

} // namespace


QnLayoutTourDialog::QnLayoutTourDialog(QWidget* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::LayoutTourDialog)
{
//     ui->setupUi(this);
//
//     QFont font;
//     font.setPixelSize(kLabelFontPixelSize);
//     font.setWeight(kLabelFontWeight);
//     ui->label->setFont(font);
//     ui->label->setProperty(style::Properties::kDontPolishFontProperty, true);
//     ui->label->setForegroundRole(QPalette::Light);
//
//     QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(this);
//     ui->treeView->setVerticalScrollBar(scrollBar->proxyScrollBar());
//
//     m_model = new QnLicenseListModel(this);
//     m_model->setExtendedStatus(true);
//
//     ui->treeView->setModel(sortModel);
//     ui->treeView->setItemDelegate(new QnLicenseListItemDelegate(this, false));
//     ui->treeView->setColumnHidden(QnLicenseListModel::ExpirationDateColumn, true);
//     ui->treeView->setColumnHidden(QnLicenseListModel::ServerColumn, true);
//     ui->treeView->setProperty(style::Properties::kSuppressHoverPropery, true);
//
//     ui->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
//
//     setResizeToContentsMode(Qt::Horizontal | Qt::Vertical);
}

QnLayoutTourDialog::~QnLayoutTourDialog()
{
}
