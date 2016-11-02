#include "license_notification_dialog.h"
#include "ui_license_notification_dialog.h"

#include <ui/delegates/license_list_item_delegate.h>
#include <ui/models/license_list_model.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/snapped_scrollbar.h>

#include <utils/common/synctime.h>

namespace {

static const int kLabelFontPixelSize = 15;
static const int kLabelFontWeight = QFont::Normal;

} // namespace


QnLicenseNotificationDialog::QnLicenseNotificationDialog(QWidget* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::LicenseNotificationDialog)
{
    ui->setupUi(this);

    QFont font;
    font.setPixelSize(kLabelFontPixelSize);
    font.setWeight(kLabelFontWeight);
    ui->label->setFont(font);
    ui->label->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->label->setForegroundRole(QPalette::Light);

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(this);
    ui->treeView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    m_model = new QnLicenseListModel(this);
    ui->treeView->setModel(m_model);
    ui->treeView->setItemDelegate(new QnLicenseListItemDelegate(this, false));
    ui->treeView->setColumnHidden(QnLicenseListModel::ExpirationDateColumn, true);
    ui->treeView->setColumnHidden(QnLicenseListModel::ServerColumn, true);
    ui->treeView->setProperty(style::Properties::kSuppressHoverPropery, true);

    ui->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    setResizeToContentsMode(Qt::Horizontal | Qt::Vertical);
}

QnLicenseNotificationDialog::~QnLicenseNotificationDialog()
{
}

void QnLicenseNotificationDialog::setLicenses(const QnLicenseList& licenses)
{
    m_model->updateLicenses(licenses);

    bool hasInvalidLicenses = std::any_of(licenses.cbegin(), licenses.cend(),
        [](const QnLicensePtr& license)
        {
            return !license->isValid();
        });

    ui->label->setText(hasInvalidLicenses
        ? tr("Some of your licenses are unavailable:")
        : tr("Some of your licenses will soon expire:"));

    adjustSize();
}
