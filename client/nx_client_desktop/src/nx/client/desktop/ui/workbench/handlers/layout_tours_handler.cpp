#include "layout_tours_handler.h"

#include <QtWidgets/QAction>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_runtime_data.h>

#include <core/resource/layout_resource.h>

#include <nx_ec/ec_api.h>

#include <ui/actions/action_manager.h>
#include <ui/dialogs/layout_tour_dialog.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <nx/client/desktop/ui/workbench/extensions/workbench_layout_tour_executor.h>
#include <nx/client/desktop/ui/workbench/extensions/workbench_layout_tour_review_controller.h>

#include <nx/utils/string.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

LayoutToursHandler::LayoutToursHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_tourExecutor(new LayoutTourExecutor(this)),
    m_reviewController(new LayoutTourReviewController(this))
{
    connect(qnLayoutTourManager, &QnLayoutTourManager::tourChanged, this,
        [this](const ec2::ApiLayoutTourData& tour)
        {
            m_tourExecutor->updateTour(tour);
            if (auto reviewLayout = m_reviewLayouts.value(tour.id))
            {
                //TODO: #GDM #3.1 dynamically change items
                reviewLayout->setData(Qn::CustomPanelTitleRole, tour.name);
                reviewLayout->setItems(QnLayoutItemDataList());
                for (auto item: tour.items)
                    addItemToReviewLayout(reviewLayout, item);
            }
        });

    connect(qnLayoutTourManager, &QnLayoutTourManager::tourRemoved, this,
        [this](const QnUuid& tourId)
        {
            m_tourExecutor->stopTour(tourId);
            if (auto reviewLayout = m_reviewLayouts.take(tourId))
                resourcePool()->removeResource(reviewLayout);
        });

    connect(action(QnActions::NewLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            QStringList usedNames;
            for (const auto& tour: qnLayoutTourManager->tours())
                usedNames << tour.name;

            ec2::ApiLayoutTourData tour;
            tour.id = QnUuid::createUuid();
            tour.name = nx::utils::generateUniqueString(
                usedNames, tr("Layout Tour"), tr("Layout Tour %1"));
            qnLayoutTourManager->addOrUpdateTour(tour);
            saveTourToServer(tour);
            reviewLayoutTour(tour);
        });

    connect(action(QnActions::RenameLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto tour = qnLayoutTourManager->tour(id);
            if (!tour.isValid())
                return;
            tour.name = parameters.argument<QString>(Qn::ResourceNameRole);
            qnLayoutTourManager->addOrUpdateTour(tour);
            saveTourToServer(tour);
        });

    connect(action(QnActions::RemoveLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            NX_EXPECT(!id.isNull());

            const auto tour = qnLayoutTourManager->tour(id);
            if (!tour.name.isEmpty())
            {
                //TODO: #GDM #3.1 add to table, fix text and buttons
                if (QnMessageBox::warning(
                    mainWindow(),
                    tr("Are you sure you want to delete %1?").arg(tour.name),
                    QString(),
                    QDialogButtonBox::Ok | QDialogButtonBox::Cancel) != QDialogButtonBox::Ok)
                {
                    return;
                }
            }

            qnLayoutTourManager->removeTour(id);
            removeTourFromServer(id);
        });

    connect(action(QnActions::LayoutTourSettingsAction), &QAction::triggered, this,
        [this]()
        {
            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto tour = qnLayoutTourManager->tour(id);
            if (!tour.isValid())
                return;

            QScopedPointer<QnLayoutTourDialog> dialog(new QnLayoutTourDialog(mainWindow()));
            dialog->loadData(tour);
            if (!dialog->exec())
                return;

            dialog->submitData(&tour);
            qnLayoutTourManager->addOrUpdateTour(tour);
            saveTourToServer(tour);
        });

    connect(action(QnActions::ToggleLayoutTourModeAction), &QAction::triggered, this,
        [this](bool toggled)
        {
            if (!toggled)
            {
                m_tourExecutor->stopCurrentTour();
                return;
            }

            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            if (id.isNull())
                m_tourExecutor->startSingleLayoutTour();
            else
                m_tourExecutor->startTour(qnLayoutTourManager->tour(id));
        });

    connect(action(QnActions::StartCurrentLayoutTourAction), &QAction::triggered, this,
        [this]
        {
            const auto id = workbench()->currentLayout()->data(Qn::LayoutTourUuidRole)
                .value<QnUuid>();
            auto tour = qnLayoutTourManager->tour(id);
            NX_EXPECT(tour.isValid());
            m_tourExecutor->startTour(tour);
        });

    connect(action(QnActions::SaveLayoutTourAction), &QAction::triggered, this,
        [this]
        {
            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto tour = qnLayoutTourManager->tour(id);
            if (!tour.isValid())
                return;
            saveTourToServer(tour);
        });

    connect(action(QnActions::ReviewLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            reviewLayoutTour(qnLayoutTourManager->tour(id));
        });

    connect(action(QnActions::EscapeHotkeyAction), &QAction::triggered, m_tourExecutor,
        &LayoutTourExecutor::stopCurrentTour);
}

LayoutToursHandler::~LayoutToursHandler()
{
}

void LayoutToursHandler::reviewLayoutTour(const ec2::ApiLayoutTourData& tour)
{
    if (!tour.isValid())
        return;

    if (auto layout = m_reviewLayouts.value(tour.id))
    {
        menu()->trigger(QnActions::OpenSingleLayoutAction, layout);
        return;
    }

    const QList<QnActions::IDType> actions{
        QnActions::StartCurrentLayoutTourAction,
        QnActions::SaveCurrentLayoutTourAction,
        QnActions::RemoveCurrentLayoutTourAction
    };

    static const float kCellAspectRatio{4.0f / 3.0f};

    const auto layout = QnLayoutResourcePtr(new QnLayoutResource());
    layout->setId(QnUuid::createUuid()); //< Layout is never saved to server
    layout->setName(tour.name);
    layout->setData(Qn::IsSpecialLayoutRole, true);
    layout->setData(Qn::LayoutIconRole, qnSkin->icon(lit("tree/videowall.png")));
    layout->setData(Qn::CustomPanelActionsRole, qVariantFromValue(actions));
    layout->setData(Qn::CustomPanelTitleRole, tour.name);
    layout->setData(Qn::CustomPanelDescriptionRole, QString());
    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadWriteSavePermission
        | Qn::AddRemoveItemsPermission));
    layout->setData(Qn::LayoutTourUuidRole, qVariantFromValue(tour.id));
    layout->setCellAspectRatio(kCellAspectRatio);

    for (auto item: tour.items)
        addItemToReviewLayout(layout, item);

    resourcePool()->addResource(layout);
    resourcePool()->markLayoutAutoGenerated(layout); //TODO: #GDM #3.1 simplify scheme for local layouts

    m_reviewLayouts.insert(tour.id, layout);

    menu()->trigger(QnActions::OpenSingleLayoutAction, layout);
}

void LayoutToursHandler::saveTourToServer(const ec2::ApiLayoutTourData& tour)
{
    NX_EXPECT(qnLayoutTourManager->tour(tour.id).isValid());
    if (const auto connection = commonModule()->ec2Connection())
    {
        connection->getLayoutTourManager(Qn::kSystemAccess)->save(tour, this,
            [](int /*reqId*/, ec2::ErrorCode /*errorCode*/) {});
    };
}

void LayoutToursHandler::removeTourFromServer(const QnUuid& tourId)
{
    if (const auto connection = commonModule()->ec2Connection())
    {
        connection->getLayoutTourManager(Qn::kSystemAccess)->remove(tourId, this,
            [](int /*reqId*/, ec2::ErrorCode /*errorCode*/) {});
    }
}

void LayoutToursHandler::addItemToReviewLayout(const QnLayoutResourcePtr& layout,
    const ec2::ApiLayoutTourItemData& item)
{
    static const int kMatrixWidth = 3;
    const int index = layout->getItems().size();

    QnLayoutItemData itemData;
    itemData.uuid = QnUuid::createUuid();
    itemData.combinedGeometry = QRect(index % kMatrixWidth, index / kMatrixWidth, 1, 1);
    itemData.flags = Qn::Pinned;
    itemData.resource.id = item.layoutId;
    qnResourceRuntimeDataManager->setLayoutItemData(itemData.uuid,
        Qn::LayoutTourItemDelayMsRole, item.delayMs);

    layout->addItem(itemData);

}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
