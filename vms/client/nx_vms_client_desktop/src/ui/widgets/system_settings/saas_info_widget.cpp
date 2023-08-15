// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_info_widget.h"
#include "ui_saas_info_widget.h"

#include <memory>

#include <QtGui/QDesktopServices>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/saas/services_sort_model.h>
#include <nx/vms/client/desktop/saas/services_usage_model.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_service_usage_helper.h>

namespace {

static constexpr QSize kPlaceholderImageSize(64, 64);
static constexpr int kServiceUsageTableMinimumColumnWidth = 100;

QFont placeholderMessageCaptionFont()
{
    static constexpr auto kPlaceholderMessageCaptionFontSize = 16;
    static constexpr auto kPlaceholderMessageCaptionFontWeight = QFont::Medium;

    QFont font;
    font.setPixelSize(kPlaceholderMessageCaptionFontSize);
    font.setWeight(kPlaceholderMessageCaptionFontWeight);
    return font;
}

QFont placeholderMessageFont()
{
    static constexpr auto kPlaceholderMessageFontSize = 14;
    static constexpr auto kPlaceholderMessageFontWeight = QFont::Normal;

    QFont font;
    font.setPixelSize(kPlaceholderMessageFontSize);
    font.setWeight(kPlaceholderMessageFontWeight);
    return font;
}

int purchasedServicesCount(const nx::vms::common::saas::ServiceManager* serviceManager)
{
    if (!NX_ASSERT(serviceManager))
        return 0;

    int servicesCount = 0;
    for (const auto& [_, saasPurchase]: serviceManager->data().services)
        servicesCount += saasPurchase.quantity;

    return servicesCount;
}

QUrl channelPartnerUrl(const nx::vms::common::saas::ServiceManager* serviceManager)
{
    return QUrl(serviceManager->data().channelPartner.webPage);
}

} // namespace

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// SaasInfoWidget::Private declaration.

struct SaasInfoWidget::Private
{
    SaasInfoWidget* const q;
    std::unique_ptr<Ui::SaasInfoWidget> ui = std::make_unique<Ui::SaasInfoWidget>();

    std::unique_ptr<saas::ServicesUsageModel> servicesUsageModel =
        std::make_unique<saas::ServicesUsageModel>(q->systemContext()->saasServiceManager());

    std::unique_ptr<saas::ServicesSortModel> servicesSortModel =
        std::make_unique<saas::ServicesSortModel>();

    void setupPlaceholderPageUi();
    void setupServicesUsagePageUi();
    void updateUi();
};

//-------------------------------------------------------------------------------------------------
// SaasInfoWidget::Private definition.

void SaasInfoWidget::Private::setupPlaceholderPageUi()
{
    const auto placeholderPixmap = qnSkin->pixmap(
        "placeholders/services_placeholder.svg", true, kPlaceholderImageSize);
    ui->placeholderIconLabel->setPixmap(qnSkin->maximumSizePixmap(placeholderPixmap));
    ui->placeholderMessageCaptionLabel->setFont(placeholderMessageCaptionFont());
    ui->placeholderMessageLabel->setFont(placeholderMessageFont());
    q->connect(ui->channelPartnerContactButton, &QPushButton::clicked, q,
        [this]
        {
            QDesktopServices::openUrl(channelPartnerUrl(q->systemContext()->saasServiceManager()));
        });
}

void SaasInfoWidget::Private::setupServicesUsagePageUi()
{
    ui->servicesUsagePage->setContentsMargins(nx::style::Metrics::kDefaultTopLevelMargins);

    servicesSortModel->setSourceModel(servicesUsageModel.get());
    servicesSortModel->sort(saas::ServicesUsageModel::ServiceNameColumn);
    ui->servicesUsageItemView->setModel(servicesSortModel.get());

    const auto header = ui->servicesUsageItemView->header();
    header->setMinimumSectionSize(kServiceUsageTableMinimumColumnWidth);
    header->setSectionResizeMode(
        saas::ServicesUsageModel::ServiceNameColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        saas::ServicesUsageModel::ServiceTypeColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(
        saas::ServicesUsageModel::TotalQantityColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        saas::ServicesUsageModel::UsedQantityColumn, QHeaderView::ResizeToContents);
}

void SaasInfoWidget::Private::updateUi()
{
    const auto serviceManager = q->systemContext()->saasServiceManager();

    const auto hasPurchasedServices = purchasedServicesCount(serviceManager) > 0;
    ui->channelPartnerContactButton->setHidden(!channelPartnerUrl(serviceManager).isValid());
    ui->stackedWidget->setCurrentWidget(hasPurchasedServices
        ? ui->servicesUsagePage
        : ui->placeholderPage);
}

//-------------------------------------------------------------------------------------------------
// ServicesWidget definition.

SaasInfoWidget::SaasInfoWidget(SystemContext* systemContext, QWidget* parent):
    base_type(parent),
    SystemContextAware(systemContext),
    d(new Private({this}))
{
    d->ui->setupUi(this);
    d->setupPlaceholderPageUi();
    d->setupServicesUsagePageUi();

    connect(systemContext->saasServiceManager(),
        &nx::vms::common::saas::ServiceManager::dataChanged,
        this,
        &SaasInfoWidget::loadDataToUi);
}

SaasInfoWidget::~SaasInfoWidget()
{
}

void SaasInfoWidget::loadDataToUi()
{
    d->updateUi();
}

bool SaasInfoWidget::hasChanges() const
{
    return false;
}

void SaasInfoWidget::applyChanges()
{
    // Widget is read-only.
}

} // namespace nx::vms::client::desktop
