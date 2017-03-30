#include "license_notification_dialog.h"
#include "ui_license_notification_dialog.h"

#include <QtCore/QSortFilterProxyModel>

#include <ui/delegates/license_list_item_delegate.h>
#include <ui/models/license_list_model.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/snapped_scrollbar.h>

#include <utils/common/synctime.h>

namespace {

static const int kLabelFontPixelSize = 15;
static const int kLabelFontWeight = QFont::Bold;

auto licenseSortPriority =
    [](const QnLicensePtr& license) -> int
    {
        QnLicense::ErrorCode code;
        license->isValid(&code);

        switch (code)
        {
            case QnLicense::NoError:
                return 2; /* Active licenses at the end. */
            case QnLicense::Expired:
                return 1; /* Expired licenses in the middle. */
            default:
                return 0; /* Erroneous licenses at the beginning. */
        }
    };

class QnLicenseNotificationSortProxyModel : public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;

public:
    QnLicenseNotificationSortProxyModel(QObject* parent = nullptr) :
        base_type(parent)
    {
    }

protected:
    virtual bool lessThan(const QModelIndex& leftIndex, const QModelIndex& rightIndex) const
    {
        QnLicensePtr left = leftIndex.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>();
        QnLicensePtr right = rightIndex.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>();

        if (!left || !right)
            return left < right;

        auto leftPriority = licenseSortPriority(left);
        auto rightPriority = licenseSortPriority(right);

        if (leftPriority != rightPriority)
            return leftPriority < rightPriority;

        return left->expirationTime() < right->expirationTime();
    }
};

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
    m_model->setExtendedStatus(true);

    auto sortModel = new QnLicenseNotificationSortProxyModel(this);
    sortModel->setSourceModel(m_model);
    sortModel->sort(0/*unused*/);

    ui->treeView->setModel(sortModel);
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
    adjustSize();
}
