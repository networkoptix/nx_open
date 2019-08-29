#include "analytics_search_widget.h"

#include <algorithm>
#include <chrono>

#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtCore/QHash>
#include <QtGui/QPalette>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QAction>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollArea>

#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_changes_listener.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/dialogs/common/message_box.h>

#include <nx/vms/client/desktop/analytics/analytics_entities_tree.h>
#include <nx/vms/client/desktop/common/dialogs/web_view_dialog.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>

#include <nx/analytics/descriptor_manager.h>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/string.h>
#include <nx/utils/std/algorithm.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget::Private

class AnalyticsSearchWidget::Private: public QObject
{
    Q_DECLARE_TR_FUNCTIONS(AnalyticsSearchWidget::Private)
    AnalyticsSearchWidget* const q;

public:
    explicit Private(AnalyticsSearchWidget* q);

    QRectF filterRect() const;
    void setFilterRect(const QRectF& value);

    bool areaSelectionEnabled() const;
    QString selectedObjectType() const;

    void resetFilters();

    void executePluginAction(
        const QnUuid& engineId,
        const QString& actionTypeId,
        const analytics::db::ObjectTrack& object,
        const QnVirtualCameraResourcePtr& camera) const;

    AnalyticsEntitiesTreeBuilder::NodePtr objectsTree;

private:
    void setupTypeSelection();
    void updateTypeMenu();

    void setupAreaSelection();
    void setAreaSelectionEnabled(bool value);
    void updateAreaButtonAppearance();

    QAction* addMenuAction(QMenu* menu, const QString& title, const QString& objectType);

    bool requestPluginActionSettings(const QJsonObject& settingsModel,
        QMap<QString, QString>& settingsValues) const;

    void setSelectedObjectType(const QString& value);

private:
    AnalyticsSearchListModel* const m_model;
    SelectableTextButton* const m_typeSelectionButton;
    SelectableTextButton* const m_areaSelectionButton;
    QMenu* const m_objectTypeMenu;
    QPointer<QAction> m_defaultAction;
    bool m_areaSelectionEnabled = false;

    using ObjectTypeDescriptors = std::map<QString, nx::vms::api::analytics::ObjectTypeDescriptor>;
    struct EngineInfo
    {
        QString name;
        ObjectTypeDescriptors objectTypes;

        bool operator<(const EngineInfo& other) const
        {
            QCollator collator;
            collator.setNumericMode(true);
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            return collator.compare(name, other.name) < 0;
        }
    };
};

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget

AnalyticsSearchWidget::AnalyticsSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new AnalyticsSearchListModel(context), parent),
    d(new Private(this))
{
    setRelevantControls(Control::defaults | Control::footersToggler);
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/analytics.png"));
    selectCameras(AbstractSearchWidget::Cameras::layout);

    connect(model(), &AbstractSearchListModel::isOnlineChanged, this,
        &AnalyticsSearchWidget::updateAllowance);
    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged, this,
        &AnalyticsSearchWidget::updateAllowance);
}

AnalyticsSearchWidget::~AnalyticsSearchWidget()
{
}

void AnalyticsSearchWidget::resetFilters()
{
    base_type::resetFilters();
    selectCameras(AbstractSearchWidget::Cameras::layout);
    d->resetFilters();
}

void AnalyticsSearchWidget::setFilterRect(const QRectF& value)
{
    d->setFilterRect(value.intersected({0, 0, 1, 1}));
}

QRectF AnalyticsSearchWidget::filterRect() const
{
    return d->filterRect();
}

bool AnalyticsSearchWidget::areaSelectionEnabled() const
{
    return d->areaSelectionEnabled();
}

QString AnalyticsSearchWidget::selectedObjectType() const
{
    return d->selectedObjectType();
}

QString AnalyticsSearchWidget::placeholderText(bool constrained) const
{
    return constrained ? tr("No objects") : tr("No objects detected");
}

QString AnalyticsSearchWidget::itemCounterText(int count) const
{
    return tr("%n objects", "", count);
}

bool AnalyticsSearchWidget::calculateAllowance() const
{
    const bool hasPermissions = model()->isOnline()
        && accessController()->hasGlobalPermission(GlobalPermission::viewArchive);

    if (!hasPermissions)
        return false;

    return !d->objectsTree->children.empty();
}

// ------------------------------------------------------------------------------------------------
// AnalyticsSearchWidget::Private

AnalyticsSearchWidget::Private::Private(AnalyticsSearchWidget* q):
    q(q),
    m_model(qobject_cast<AnalyticsSearchListModel*>(q->model())),
    m_typeSelectionButton(q->createCustomFilterButton()),
    m_areaSelectionButton(q->createCustomFilterButton()),
    m_objectTypeMenu(q->createDropdownMenu())
{
    NX_CRITICAL(m_model);

    auto updateObjectTypesMenuOperation = new nx::utils::PendingOperation(
        [this]
        {
            auto analyticsObjectsSearchTreeBuilder =
                this->q->commonModule()->instance<AnalyticsObjectsSearchTreeBuilder>();
            objectsTree = analyticsObjectsSearchTreeBuilder->objectTypesTree();

            this->q->updateAllowance();
            if (this->q->isAllowed())
                updateTypeMenu();
        },
        100ms,
        this);
    updateObjectTypesMenuOperation->fire();
    updateObjectTypesMenuOperation->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);

    connect(q->commonModule()->instance<AnalyticsObjectsSearchTreeBuilder>(),
        &AnalyticsObjectsSearchTreeBuilder::objectTypesTreeChanged,
        updateObjectTypesMenuOperation,
        &nx::utils::PendingOperation::requestOperation);

    connect(m_model, &AbstractSearchListModel::isOnlineChanged,
        updateObjectTypesMenuOperation,
        &nx::utils::PendingOperation::requestOperation);

    setupTypeSelection();
    setupAreaSelection();

    connect(q, &AbstractSearchWidget::textFilterChanged,
        m_model, &AnalyticsSearchListModel::setFilterText);

    connect(m_model, &AnalyticsSearchListModel::pluginActionRequested,
        this, &Private::executePluginAction);
}

QRectF AnalyticsSearchWidget::Private::filterRect() const
{
    return m_model->filterRect();
}

void AnalyticsSearchWidget::Private::setFilterRect(const QRectF& value)
{
    if (qFuzzyEquals(m_model->filterRect(), value))
        return;

    m_model->setFilterRect(value);
    updateAreaButtonAppearance();

    emit q->filterRectChanged(m_model->filterRect());
}

bool AnalyticsSearchWidget::Private::areaSelectionEnabled() const
{
    return m_areaSelectionEnabled;
}

QString AnalyticsSearchWidget::Private::selectedObjectType() const
{
    return m_model->selectedObjectType();
}

void AnalyticsSearchWidget::Private::setSelectedObjectType(const QString& value)
{
    if (m_model->selectedObjectType() == value)
        return;

    m_model->setSelectedObjectType(value);
    emit q->selectedObjectTypeChanged();
}

void AnalyticsSearchWidget::Private::resetFilters()
{
    m_typeSelectionButton->deactivate();
}

void AnalyticsSearchWidget::Private::setupTypeSelection()
{
    m_typeSelectionButton->setDeactivatedText(tr("Any type"));
    m_typeSelectionButton->setIcon(qnSkin->icon("text_buttons/analytics.png"));

    m_typeSelectionButton->setMenu(m_objectTypeMenu);

    connect(m_typeSelectionButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated && m_defaultAction)
                m_defaultAction->trigger();
        });
}

void AnalyticsSearchWidget::Private::updateTypeMenu()
{
    NX_ASSERT(!objectsTree->children.empty());

    m_typeSelectionButton->setDeactivatedText(tr("Any type"));
    m_typeSelectionButton->setIcon(qnSkin->icon("text_buttons/analytics.png"));

    const QString currentSelection = m_model->selectedObjectType();
    bool currentSelectionStillAvailable = false;

    auto addItemRecursive = nx::utils::y_combinator(
        [this, currentSelection, &currentSelectionStillAvailable]
        (auto addItemRecursive, auto parent, auto root) -> void
            {
                using NodeType = AnalyticsEntitiesTreeBuilder::NodeType;

                for (auto node: root->children)
                {
                    // Leaf node - object type.
                    if (node->nodeType == NodeType::objectType)
                    {
                        addMenuAction(parent, node->text, node->entityId);

                        if (!currentSelectionStillAvailable
                            && currentSelection == node->entityId)
                        {
                            currentSelectionStillAvailable = true;
                        }
                    }
                    else
                    {
                        NX_ASSERT(!node->children.empty());
                        auto menu = parent->addMenu(node->text);
                        q->fixMenuFlags(menu);

                        addItemRecursive(menu, node);
                    }
                }
            });

    WidgetUtils::clearMenu(m_objectTypeMenu);

    m_defaultAction = addMenuAction(m_objectTypeMenu, tr("Any type"), {});
    m_objectTypeMenu->addSeparator();
    addItemRecursive(m_objectTypeMenu, objectsTree);

    if (!currentSelectionStillAvailable)
        m_typeSelectionButton->deactivate();
}

void AnalyticsSearchWidget::Private::setupAreaSelection()
{
    m_areaSelectionButton->setAccented(true);
    m_areaSelectionButton->setDeactivatedText(tr("Select area"));
    m_areaSelectionButton->setIcon(qnSkin->icon("text_buttons/area.png"));
    m_areaSelectionButton->hide();

    connect(q, &AbstractSearchWidget::cameraSetChanged, this,
        [this](const QnVirtualCameraResourceSet& cameras)
        {
            setAreaSelectionEnabled(!cameras.empty());
            if (q->selectedCameras() != Cameras::current)
                m_areaSelectionButton->deactivate();
        });

    connect(m_areaSelectionButton, &SelectableTextButton::stateChanged,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                setFilterRect({});
        });

    connect(m_areaSelectionButton, &SelectableTextButton::clicked, this,
        [this]()
        {
            m_areaSelectionButton->setText(tr("Select some area on the video..."));
            m_areaSelectionButton->setAccented(true);
            m_areaSelectionButton->setState(SelectableTextButton::State::unselected);
            q->selectCameras(Cameras::current);
            emit q->areaSelectionRequested({});
        });
}

void AnalyticsSearchWidget::Private::setAreaSelectionEnabled(bool value)
{
    if (m_areaSelectionEnabled == value)
        return;

    m_areaSelectionEnabled = value;
    m_areaSelectionButton->setVisible(m_areaSelectionEnabled);
    emit q->areaSelectionEnabledChanged(m_areaSelectionEnabled, {});
}

void AnalyticsSearchWidget::Private::updateAreaButtonAppearance()
{
    m_areaSelectionButton->setState(m_model->filterRect().isValid()
        ? SelectableTextButton::State::unselected
        : SelectableTextButton::State::deactivated);

    m_areaSelectionButton->setAccented(false);
    m_areaSelectionButton->setText(tr("In selected area"));
}

QAction* AnalyticsSearchWidget::Private::addMenuAction(
    QMenu* menu, const QString& title, const QString& objectType)
{
    auto action = menu->addAction(title);
    connect(action, &QAction::triggered, this,
        [this, action, objectType]()
        {
            m_typeSelectionButton->setText(action->text());
            m_typeSelectionButton->setState(objectType.isEmpty()
                ? SelectableTextButton::State::deactivated
                : SelectableTextButton::State::unselected);

            setSelectedObjectType(objectType);
        });

    return action;
}

void AnalyticsSearchWidget::Private::executePluginAction(
    const QnUuid& engineId,
    const QString& actionTypeId,
    const analytics::db::ObjectTrack& track,
    const QnVirtualCameraResourcePtr& camera) const
{
    const auto server = q->commonModule()->currentServer();
    if (!server || !server->restConnection())
        return;

    const nx::analytics::ActionTypeDescriptorManager descriptorManager(q->commonModule());
    const auto actionDescriptor = descriptorManager.descriptor(actionTypeId);
    if (!actionDescriptor)
        return;

    AnalyticsAction actionData;
    actionData.engineId = engineId;
    actionData.actionId = actionDescriptor->id;
    actionData.objectTrackId = track.id;
    actionData.timestampUs = track.firstAppearanceTimeUs;
    actionData.deviceId = camera->getId();

    if (!actionDescriptor->parametersModel.isEmpty())
    {
        // Show dialog asking to enter required parameters.
        if (!requestPluginActionSettings(actionDescriptor->parametersModel, actionData.params))
            return;
    }

    const auto resultCallback =
        [this](bool success, rest::Handle /*requestId*/, QnJsonRestResult result)
        {
            if (result.error != QnRestResult::NoError)
            {
                QnMessageBox::warning(q->mainWindowWidget(), tr("Failed to execute plugin action"),
                    result.errorString);
                return;
            }

            if (!success)
                return;

            const auto reply = result.deserialized<AnalyticsActionResult>();
            if (!reply.messageToUser.isEmpty())
                QnMessageBox::success(q->mainWindowWidget(), reply.messageToUser);

            if (!reply.actionUrl.isEmpty())
                WebViewDialog::showUrl(QUrl(reply.actionUrl));
        };


    server->restConnection()->executeAnalyticsAction(
        actionData, nx::utils::guarded(this, resultCallback), thread());
}

bool AnalyticsSearchWidget::Private::requestPluginActionSettings(const QJsonObject& settingsModel,
    QMap<QString, QString>& settingsValues) const
{
    QnMessageBox parametersDialog(q->mainWindowWidget());
    parametersDialog.addButton(QDialogButtonBox::Ok);
    parametersDialog.addButton(QDialogButtonBox::Cancel);
    parametersDialog.setText(tr("Enter parameters"));
    parametersDialog.setInformativeText(tr("Action requires some parameters to be filled."));
    parametersDialog.setIcon(QnMessageBoxIcon::Information);

    auto view = new QQuickWidget(qnClientCoreModule->mainQmlEngine(), &parametersDialog);
    view->setClearColor(parametersDialog.palette().window().color());
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->setSource(QUrl("Nx/InteractiveSettings/SettingsView.qml"));
    const auto root = view->rootObject();
    NX_ASSERT(root);

    if (!root)
        return false;

    QMetaObject::invokeMethod(
        root,
        "loadModel",
        Qt::DirectConnection,
        Q_ARG(QVariant, settingsModel.toVariantMap()),
        Q_ARG(QVariant, {}));

    auto panel = new QScrollArea(&parametersDialog);
    panel->setFixedHeight(400);
    auto layout = new QHBoxLayout(panel);
    layout->addWidget(view);

    parametersDialog.addCustomWidget(panel, QnMessageBox::Layout::Main);
    if (parametersDialog.exec() != QDialogButtonBox::Ok)
        return false;

    QVariant result;
    QMetaObject::invokeMethod(
        root,
        "getValues",
        Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, result));

    settingsValues.clear();
    const auto resultMap = result.value<QVariantMap>();
    for (auto iter = resultMap.cbegin(); iter != resultMap.cend(); ++iter)
        settingsValues.insert(iter.key(), iter.value().toString());

    return true;
}

} // namespace nx::vms::client::desktop
