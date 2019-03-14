#include "camera_advanced_param_widgets_manager.h"

#include <set>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>

#include <boost/range.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include <ui/style/helper.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/range_adapters.h>

#include "camera_advanced_param_widget_factory.h"

namespace {

static constexpr int kParameterLabelWidth = 120;

} // namespace

using ConditionType = QnCameraAdvancedParameterCondition::ConditionType;
using Dependency = QnCameraAdvancedParameterDependency;
using DependencyType = QnCameraAdvancedParameterDependency::DependencyType;

namespace nx::vms::client::desktop {

CameraAdvancedParamWidgetsManager::CameraAdvancedParamWidgetsManager(
    QTreeWidget* groupWidget,
    QStackedWidget* contentsWidget,
    QObject* parent)
    :
    QObject(parent),
    m_groupWidget(groupWidget),
    m_contentsWidget(contentsWidget)
{
}

void CameraAdvancedParamWidgetsManager::clear()
{
    m_groupWidget->disconnect(this);

    for (const auto widget: m_paramWidgetsById)
        widget->disconnect(this);

    m_paramWidgetsById.clear();
    m_paramLabelsById.clear();
    m_parametersById.clear();
    m_handlerChains.clear();
    m_groupWidget->clear();
    m_itemsAvailableInOffline.clear();
    while (m_contentsWidget->count() > 0)
        m_contentsWidget->removeWidget(m_contentsWidget->widget(0));
}


void CameraAdvancedParamWidgetsManager::displayParams(const QnCameraAdvancedParams& params)
{
    clear();

    for (const QnCameraAdvancedParamGroup &group : params.groups)
        createGroupWidgets(group);

    const auto currentItemChanged =
        [this](QTreeWidgetItem* current)
        {
            if (!current)
                return;

            const auto target = current->data(0, Qt::UserRole).value<QWidget*>();
            if (target && m_contentsWidget->children().contains(target))
                m_contentsWidget->setCurrentWidget(target);
        };

    connect(m_groupWidget, &QTreeWidget::currentItemChanged, this, currentItemChanged);
    currentItemChanged(m_groupWidget->currentItem());
}

void CameraAdvancedParamWidgetsManager::loadValues(
    const QnCameraAdvancedParamValueList& params,
    bool packetMode)
{
    if (packetMode)
    {
        // Disable all parameter widgets, we'll enable only those that are on the list.
        for (auto [parameterId, widget]: nx::utils::constKeyValueRange(m_paramWidgetsById))
        {
            // TODO: #dmishin some attribute like `isEnabledByDefault` is needed.
            const bool isButton = m_parametersById[parameterId].dataType
                == QnCameraAdvancedParameter::DataType::Button;

            const bool isPtrControl = m_parametersById[parameterId].dataType
                == QnCameraAdvancedParameter::DataType::PtrControl;

            const bool isSlider = m_parametersById[parameterId].dataType
                == QnCameraAdvancedParameter::DataType::SliderControl;

            // We always show buttons, joysticks and sliders since their "values"
            // don't arrive from server.
            widget->setEnabled(isButton || isPtrControl || isSlider);
        }
    }

    // Disconnect all watches to not trigger handler chains.
    m_handlerChainConnections = {};

    // Set widget values and store initial values that should be kept.
    QMap<QString, QString> valuesToKeep;
    for (const auto& param: params)
    {
        if (!m_paramWidgetsById.contains(param.id))
            continue;

        const auto widget = m_paramWidgetsById.value(param.id);
        widget->setValue(param.value);
        widget->setEnabled(true);

        if (+m_parametersById[param.id].keepInitialValue)
            valuesToKeep[param.id] = param.value;
    }

    runAllHandlerChains();

    // Connect handler chains to watched values.
    for (auto itr = m_handlerChains.cbegin(); itr != m_handlerChains.cend(); ++itr)
    {
        const auto& watch = itr.key();
        const auto& handlerChains = itr.value();
        for (auto& func: handlerChains)
        {
            auto watchWidget = m_paramWidgetsById.value(watch);
            m_handlerChainConnections << connect(
                watchWidget,
                &AbstractCameraAdvancedParamWidget::valueChanged,
                func);
        }
    }

    // Restore initial values for some widgets.
    for (const auto& paramId: valuesToKeep.keys())
    {
        auto widget = m_paramWidgetsById.value(paramId);
        if (!widget)
            continue;

        widget->setValue(valuesToKeep[paramId]);
    }

    for (const auto& param: params)
    {
        auto widget = m_paramWidgetsById.value(param.id);
        connect(
            widget,
            &AbstractCameraAdvancedParamWidget::valueChanged,
            this,
            &CameraAdvancedParamWidgetsManager::paramValueChanged,
            Qt::UniqueConnection);
    }
}

std::optional<QString> CameraAdvancedParamWidgetsManager::parameterValue(
    const QString& parameterId) const
{
    const auto widget = m_paramWidgetsById.value(parameterId);
    if (!widget)
        return std::nullopt;
    return widget->value();
}

void CameraAdvancedParamWidgetsManager::updateParametersVisibility(ParameterVisibility visibility)
{
    const bool visible = (visibility == ParameterVisibility::showAll);

    for (auto it = m_parametersById.cbegin(); it != m_parametersById.cend(); ++it)
    {
        if (it->availableInOffline)
            continue; //< Nothing to do with such parameters.

        if (const auto label = m_paramLabelsById.value(it.key()))
            label->setVisible(visible);
        if (const auto widget = m_paramWidgetsById.value(it.key()))
            widget->setVisible(visible);
    }

    const auto updateItemVisibility =
        [this, visible](QTreeWidgetItem* item, const auto& updateItemVisibility) -> void
        {
            if (!m_itemsAvailableInOffline.contains(item))
                item->setHidden(!visible);

            for (int i = item->childCount() - 1; i >= 0; --i)
                updateItemVisibility(item->child(i), updateItemVisibility);
        };

    for (int i = m_groupWidget->topLevelItemCount() - 1; i >= 0; --i)
        updateItemVisibility(m_groupWidget->topLevelItem(i), updateItemVisibility);
}

bool CameraAdvancedParamWidgetsManager::hasItemsAvailableInOffline() const
{
    return !m_itemsAvailableInOffline.isEmpty();
}

bool CameraAdvancedParamWidgetsManager::hasValidValues(
    const QnCameraAdvancedParamGroup& group) const
{
    bool hasValidParameter = boost::algorithm::any_of(
        group.params,
        [](const QnCameraAdvancedParameter &param) { return param.isValid(); });
    if (hasValidParameter)
        return true;
    return boost::algorithm::any_of(
        group.groups,
        [this](const QnCameraAdvancedParamGroup &group) { return hasValidValues(group); });
}


void CameraAdvancedParamWidgetsManager::createGroupWidgets(
    const QnCameraAdvancedParamGroup& group, QTreeWidgetItem* parentItem, bool* availableInOffline)
{
    if (availableInOffline)
        *availableInOffline = false;

    if (!hasValidValues(group))
        return;

    bool itemAvailableInOffline = false;

    QTreeWidgetItem* item;
    if (parentItem)
    {
        item = new QTreeWidgetItem(parentItem);
        parentItem->addChild(item);
    }
    else
    {
        item = new QTreeWidgetItem(m_groupWidget);
        m_groupWidget->addTopLevelItem(item);
    }
    item->setData(0, Qt::DisplayRole, group.name);
    item->setData(0, Qt::ToolTipRole, group.description);
    item->setExpanded(true);

    if (group.params.empty() && !group.groups.empty())
    {
        item->setFlags(Qt::ItemIsEnabled);
    }
    else
    {
        QWidget* contentsPage = createContentsPage(group.name, group.params);
        m_contentsWidget->addWidget(contentsPage);
        item->setData(0, Qt::UserRole, qVariantFromValue(contentsPage));

        if (!m_groupWidget->currentItem())
            m_groupWidget->setCurrentItem(item);
    }

    for (const QnCameraAdvancedParameter& param: group.params)
    {
        if (param.availableInOffline)
        {
            itemAvailableInOffline = true;
            break;
        }
    }

    for (const QnCameraAdvancedParamGroup& subGroup: group.groups)
    {
        bool anyGroupAvailableInOffline = false;
        createGroupWidgets(subGroup, item, &anyGroupAvailableInOffline);
        itemAvailableInOffline |= anyGroupAvailableInOffline;
    }

    if (itemAvailableInOffline)
    {
        m_itemsAvailableInOffline.insert(item);
        if (availableInOffline)
            *availableInOffline = true;
    }
}

QWidget* CameraAdvancedParamWidgetsManager::createContentsPage(
    const QString& name,
    const std::vector<QnCameraAdvancedParameter>& params)
{
    auto page = createWidgetsForPage(name, params);
    setUpDependenciesForPage(params);
    // Make initial call to handle default values.
    runAllHandlerChains();

    return page;
}

QWidget* CameraAdvancedParamWidgetsManager::createWidgetsForPage(
    const QString &name,
    const std::vector<QnCameraAdvancedParameter> &params)
{
    auto page = new QWidget(m_contentsWidget);
    auto pageLayout = new QHBoxLayout(page);
    pageLayout->setContentsMargins(style::Metrics::kDefaultTopLevelMargins);

    auto groupBox = new QGroupBox(page);
    groupBox->setFlat(true);
    groupBox->setTitle(name);
    groupBox->setLayout(new QHBoxLayout(groupBox));
    pageLayout->addWidget(groupBox);

    auto scrollArea = new QScrollArea(groupBox);
    scrollArea->setWidgetResizable(true);
    groupBox->layout()->addWidget(scrollArea);

    auto scrollAreaWidgetContents = new QWidget();
    scrollAreaWidgetContents->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto gridLayout = new QGridLayout(scrollAreaWidgetContents);
    scrollArea->setWidget(scrollAreaWidgetContents);

    // Container for a group of 'compact' widgets.
    QHBoxLayout* compactLayout = nullptr;
    auto checkFinishGroup = [&compactLayout]()
        {
            if (compactLayout)
            {
                compactLayout->addStretch(1);
                compactLayout = nullptr;
            }
        };

    for (const auto& param : params)
    {
        m_parametersById[param.id] = param;
        auto widget = QnCameraAdvancedParamWidgetFactory::createWidget(param, scrollAreaWidgetContents);
        if (!widget)
            continue;

        const auto row = gridLayout->rowCount();

        if (param.dataType == QnCameraAdvancedParameter::DataType::Separator)
        {
            checkFinishGroup();
            gridLayout->addWidget(widget, row, 0, 1, 2);
            continue;
        }

        if (param.compact)
        {
            if (!compactLayout)
            {
                // Creating container for compact controls.
                compactLayout = new QHBoxLayout(m_contentsWidget);
                compactLayout->addWidget(widget, 0, Qt::AlignRight);
                gridLayout->addLayout(compactLayout, row, 0, 1, 2, Qt::AlignVCenter);
            }
            else
            {
                // Adding a widget to existing contaner for compact control group.
                compactLayout->addWidget(widget, 0, Qt::AlignRight);
            }
        }
        else
        {
            checkFinishGroup();

            if (param.dataType == QnCameraAdvancedParameter::DataType::Button)
            {
                gridLayout->addWidget(widget, row, 0, 2, 1, Qt::AlignVCenter);
            }
            else
            {
                auto label = new QLabel(scrollAreaWidgetContents);
                label->setToolTip(param.description);
                setLabelText(label, param, param.range);
                label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                label->setBuddy(widget);
                gridLayout->addWidget(label, row, 0);
                m_paramLabelsById[param.id] = label;
                label->setFixedWidth(kParameterLabelWidth);
                label->setWordWrap(true);
                gridLayout->addWidget(widget, row, 1, 1, 1, Qt::AlignVCenter);
            }
        }

        m_paramWidgetsById[param.id] = widget;

        // Widget is disabled until it receives correct value.
        if (QnCameraAdvancedParameter::dataTypeHasValue(param.dataType) && !param.isCustomControl())
        {
            widget->setEnabled(false);
        }
        else
        {
            connect(widget, &AbstractCameraAdvancedParamWidget::valueChanged, this,
                &CameraAdvancedParamWidgetsManager::paramValueChanged);
        }

        if (!param.notes.isEmpty() && !param.compact)
        {
            auto label = new QLabel(scrollAreaWidgetContents);
            label->setWordWrap(true);
            label->setText(param.notes);
            gridLayout->addWidget(label, row + 1, 1);
        }
    }
    checkFinishGroup();

    gridLayout->setColumnStretch(1, 1);

    return page;
}

void CameraAdvancedParamWidgetsManager::setUpDependenciesForPage(
    const std::vector<QnCameraAdvancedParameter>& params)
{
    for (const auto& param : params)
    {
        if (param.dependencies.empty())
            continue;

        std::set<QString> watches;
        HandlerChains handlerChains;

        for (const auto& dependency : param.dependencies)
        {
            for (const auto& cond : dependency.conditions)
                watches.insert(cond.paramId);

            handlerChains[dependency.type].push_back(
                makeDependencyHandler(dependency, param));
        }

        auto runHandlerChains = makeHandlerChainLauncher(handlerChains);
        for (const auto& watch : watches)
        {
            if (auto widget = m_paramWidgetsById.value(watch))
                m_handlerChains[watch].push_back(runHandlerChains);
        }
    }
}

void CameraAdvancedParamWidgetsManager::runAllHandlerChains()
{
    // Run handler chains.
    for (const auto& handlerChains: m_handlerChains)
    {
        for (auto& func: handlerChains)
            func();
    }
}

CameraAdvancedParamWidgetsManager::DependencyHandler
    CameraAdvancedParamWidgetsManager::makeDependencyHandler(
        const QnCameraAdvancedParameterDependency& dependency,
        const QnCameraAdvancedParameter& parameter) const
{
    return
        [dependency, parameter, this]() -> bool
        {
            const auto paramId = parameter.id;
            auto widget = m_paramWidgetsById.value(paramId);
            if (!widget)
                return false;

            bool allConditionsSatisfied = std::all_of(
                dependency.conditions.cbegin(), dependency.conditions.cend(),
                [this](const QnCameraAdvancedParameterCondition& condition)
            {
                using ConditionType =
                    QnCameraAdvancedParameterCondition::ConditionType;

                if (condition.type == ConditionType::present)
                    return m_paramWidgetsById.contains(condition.paramId);

                if (condition.type == ConditionType::notPresent)
                    return !m_paramWidgetsById.contains(condition.paramId);

                auto widget = m_paramWidgetsById.value(condition.paramId);
                return widget && condition.checkValue(widget->value());
            });

            // TODO: #dmishin move this somewhere.
            if (dependency.type == DependencyType::show)
            {
                if (auto label = m_paramLabelsById.value(paramId))
                    label->setHidden(!allConditionsSatisfied);

                widget->setHidden(!allConditionsSatisfied);
            }
            else if (dependency.type == DependencyType::range)
            {
                if (!allConditionsSatisfied)
                    return false;

                if (!dependency.valuesToAddToRange.isEmpty())
                {
                    auto range = widget->range();
                    for (const auto& valueToAdd: dependency.valuesToAddToRange)
                    {
                        if (!range.contains(valueToAdd))
                            range.push_back(valueToAdd);
                    }

                    widget->setRange(range.join(L','));
                }

                if (!dependency.valuesToRemoveFromRange.isEmpty())
                {
                    QStringList result;
                    const auto range = widget->range();
                    for (const auto& value: range)
                    {
                        if (!dependency.valuesToRemoveFromRange.contains(value))
                            result.push_back(value);
                    }

                    widget->setRange(result.join(L','));
                }

                if (!dependency.range.isEmpty())
                {
                    NX_ASSERT(dependency.valuesToAddToRange.isEmpty()
                        && dependency.valuesToRemoveFromRange.isEmpty());

                    widget->setRange(dependency.range);
                    if (parameter.bindDefaultToMinimum)
                    {
                        const auto minMax = dependency.range.split(L',');
                        if (!minMax.isEmpty())
                            widget->setValue(minMax.first().trimmed());
                    }

                    auto label = dynamic_cast<QLabel*>(m_paramLabelsById[paramId]);
                    if (!label)
                        return false;

                    setLabelText(label, parameter, dependency.range);
                }
            }
            else if (dependency.type == DependencyType::trigger)
            {
                return false; //< Do nothing. All work will be done by other handlers
            }

            return allConditionsSatisfied;
        };
}

CameraAdvancedParamWidgetsManager::HandlerChainLauncher
CameraAdvancedParamWidgetsManager::makeHandlerChainLauncher(
    const HandlerChains& handlerChains) const
{
    return
        [handlerChains]()
        {
            for (const auto& chainPair : handlerChains)
            {
                auto& chain = chainPair.second;
                for (const auto& handler : chain)
                {
                    if (handler())
                        break;
                }
            }
        };
}

void CameraAdvancedParamWidgetsManager::setLabelText(
    QLabel* label,
    const QnCameraAdvancedParameter& parameter,
    const QString& range) const
{
    if (!NX_ASSERT(label))
        return;

    if (!parameter.showRange)
    {
        label->setText(parameter.name);
        return;
    }

    const auto rangeSplit = range.split(L',');
    if (rangeSplit.size() != 2)
        return;

    NX_ASSERT(parameter.dataType == QnCameraAdvancedParameter::DataType::Number);
    label->setText(QStringLiteral("%1 (%2-%3)").arg(
        parameter.name, rangeSplit[0].trimmed(), rangeSplit[1].trimmed()));
}

} // namespace nx::vms::client::desktop
