// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_notification_dialog.h"
#include "ui_license_notification_dialog.h"

#include <QtCore/QSortFilterProxyModel>

#include <common/common_module.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/license/validator.h>
#include <ui/delegates/license_list_item_delegate.h>
#include <ui/models/license_list_model.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client::desktop;

namespace {

static const auto kLabelFontWeight = QFont::Bold;

auto licenseSortPriority =
    [](nx::vms::license::Validator* validator, const QnLicensePtr& license) -> int
    {
        auto code = validator->validate(license);
        switch (code)
        {
            case nx::vms::license::QnLicenseErrorCode::NoError:
                return 2; /* Active licenses at the end. */
            case nx::vms::license::QnLicenseErrorCode::Expired:
            case nx::vms::license::QnLicenseErrorCode::TemporaryExpired:
                return 1; /* Expired licenses in the middle. */
            default:
                return 0; /* Erroneous licenses at the beginning. */
        }
    };

class QnLicenseNotificationSortProxyModel:
    public QSortFilterProxyModel,
    public nx::vms::client::core::CommonModuleAware
{
    using base_type = QSortFilterProxyModel;

public:
    QnLicenseNotificationSortProxyModel(QObject* parent = nullptr) :
        base_type(parent),
        validator(new nx::vms::license::Validator(systemContext(), this))
    {
    }

protected:
    virtual bool lessThan(const QModelIndex& leftIndex, const QModelIndex& rightIndex) const
    {
        QnLicensePtr left = leftIndex.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>();
        QnLicensePtr right = rightIndex.data(QnLicenseListModel::LicenseRole).value<QnLicensePtr>();

        if (!left || !right)
            return left < right;

        auto leftPriority = licenseSortPriority(validator, left);
        auto rightPriority = licenseSortPriority(validator, right);

        if (leftPriority != rightPriority)
            return leftPriority < rightPriority;

        return left->expirationTime() < right->expirationTime();
    }

private:
    nx::vms::license::Validator* validator;
};

} // namespace


QnLicenseNotificationDialog::QnLicenseNotificationDialog(QWidget* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::LicenseNotificationDialog)
{
    ui->setupUi(this);

    QFont font;
    font.setPixelSize(fontConfig()->large().pixelSize());
    font.setWeight(kLabelFontWeight);
    ui->label->setFont(font);
    ui->label->setProperty(nx::style::Properties::kDontPolishFontProperty, true);
    ui->label->setForegroundRole(QPalette::Light);

    SnappedScrollBar* scrollBar = new SnappedScrollBar(this);
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
    ui->treeView->setProperty(nx::style::Properties::kSuppressHoverPropery, true);

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
