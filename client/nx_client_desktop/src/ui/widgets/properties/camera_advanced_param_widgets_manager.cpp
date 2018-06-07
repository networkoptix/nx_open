#include "camera_advanced_param_widgets_manager.h"

#include <set>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>

#include <boost/range.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include <ui/style/helper.h>
#include <ui/widgets/properties/camera_advanced_param_widget_factory.h>

#include <nx/utils/log/assert.h>

namespace {

static constexpr int kParameterLabelWidth = 120;

} // namespace

using ConditionType = QnCameraAdvancedParameterCondition::ConditionType;
using Dependency = QnCameraAdvancedParameterDependency;
using DependencyType = QnCameraAdvancedParameterDependency::DependencyType;

QnCameraAdvancedParamWidgetsManager::QnCameraAdvancedParamWidgetsManager(QTreeWidget* groupWidget, QStackedWidget* contentsWidget, QObject* parent /*= NULL*/):
    QObject(parent),
    m_groupWidget(groupWidget),
    m_contentsWidget(contentsWidget)
{
}

void QnCameraAdvancedParamWidgetsManager::clear()
{
    disconnect(m_groupWidget, NULL, this, NULL);

    for (QnAbstractCameraAdvancedParamWidget* widget : m_paramWidgetsById)
        disconnect(widget, NULL, this, NULL);

    m_paramWidgetsById.clear();
    m_paramLabelsById.clear();
    m_parametersById.clear();
    m_handlerChains.clear();
    m_groupWidget->clear();
    while (m_contentsWidget->count() > 0)
        m_contentsWidget->removeWidget(m_contentsWidget->widget(0));
}


void QnCameraAdvancedParamWidgetsManager::displayParams(const QnCameraAdvancedParams& params)
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

void QnCameraAdvancedParamWidgetsManager::loadValues(
    const QnCameraAdvancedParamValueList &params)
{
    for (const auto param : params)
    {
        qDebug() << "$$$$$$$$$$$$$$ PARAMETER:" << param.id << param.value;
    }

    // Disconnect all watches to not trigger handler chains.
    m_handlerChainConnections = QnDisconnectHelper::create();

    // Set widget values and store initial values that should be kept.
    QMap<QString, QString> valuesToKeep;
    for (const auto& param: params)
    {
        if (!m_paramWidgetsById.contains(param.id))
            continue;

        auto widget = m_paramWidgetsById[param.id];

        qDebug() << "########## PARAMETER (SETTING VALUE):" << param.id << param.value;
        widget->setValue(param.value);
        widget->setEnabled(true);

        if (m_parametersById[param.id].keepInitialValue)
            valuesToKeep[param.id] = param.value;
    }

    runAllHandlerChains();

    qDebug() << "@@@ VALUE:"<< m_paramWidgetsById[lit("SECONDARY%media/videoprofile/FrameRate")]->value();

    // Connect handler chains to watched values.
    for (auto itr = m_handlerChains.cbegin(); itr != m_handlerChains.cend(); ++itr)
    {
        const auto& watch = itr.key();
        const auto& handlerChains = itr.value();
        for (auto& func: handlerChains)
        {
            auto watchWidget = m_paramWidgetsById[watch];
            m_handlerChainConnections->add(
                connect(
                    watchWidget,
                    &QnAbstractCameraAdvancedParamWidget::valueChanged,
                    func));
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
        auto widget = m_paramWidgetsById[param.id];
        disconnect(
            widget,
            &QnAbstractCameraAdvancedParamWidget::valueChanged,
            this,
            &QnCameraAdvancedParamWidgetsManager::paramValueChanged);

        connect(
            widget,
            &QnAbstractCameraAdvancedParamWidget::valueChanged,
            this,
            &QnCameraAdvancedParamWidgetsManager::paramValueChanged);
    }
}

std::optional<QString> QnCameraAdvancedParamWidgetsManager::parameterValue(
    const QString & parameterId) const
{
    if (!m_paramWidgetsById.contains(parameterId))
        return std::nullopt;

    return m_paramWidgetsById[parameterId]->value();
}

bool QnCameraAdvancedParamWidgetsManager::hasValidValues(const QnCameraAdvancedParamGroup &group) const
{
    bool hasValidParameter = boost::algorithm::any_of(group.params, [](const QnCameraAdvancedParameter &param) { return param.isValid(); });
    if (hasValidParameter)
        return true;
    return boost::algorithm::any_of(group.groups, [this](const QnCameraAdvancedParamGroup &group) { return hasValidValues(group); });
}


void QnCameraAdvancedParamWidgetsManager::createGroupWidgets(const QnCameraAdvancedParamGroup &group, QTreeWidgetItem* parentItem)
{
    if (!hasValidValues(group))
        return;

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

    for (const QnCameraAdvancedParamGroup &subGroup : group.groups)
        createGroupWidgets(subGroup, item);
}

QWidget* QnCameraAdvancedParamWidgetsManager::createContentsPage(
    const QString& name,
    const std::vector<QnCameraAdvancedParameter>& params)
{
    auto page = createWidgetsForPage(name, params);
    setUpDependenciesForPage(params);
    // Make initial call to handle default values.
    runAllHandlerChains();

    return page;
}

QWidget* QnCameraAdvancedParamWidgetsManager::createWidgetsForPage(
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

    for (const auto& param : params)
    {
        m_parametersById[param.id] = param;
        auto widget = QnCameraAdvancedParamWidgetFactory::createWidget(param, scrollAreaWidgetContents);
        if (!widget)
            continue;

        const auto row = gridLayout->rowCount();

        if (param.dataType == QnCameraAdvancedParameter::DataType::Separator)
        {
            gridLayout->addWidget(widget, row, 0, 1, 2);
            continue;
        }

        // Column span for a control
        int colSpan = 1;

        bool hasLabel = false;

        if (param.dataType != QnCameraAdvancedParameter::DataType::Button
            && param.dataType != QnCameraAdvancedParameter::DataType::LensControl)
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
            hasLabel = true;
        }
        else
        {
            hasLabel = false;
            colSpan = 2;
        }

        gridLayout->addWidget(widget, row, hasLabel ? 1:0, 1, colSpan, Qt::AlignVCenter);
        m_paramWidgetsById[param.id] = widget;

        /* Widget is disabled until it receives correct value. */
        if (QnCameraAdvancedParameter::dataTypeHasValue(param.dataType))
        {
            widget->setEnabled(false);
        }
        else
        {
            connect(widget, &QnAbstractCameraAdvancedParamWidget::valueChanged, this,
                &QnCameraAdvancedParamWidgetsManager::paramValueChanged);
        }

        if (!param.notes.isEmpty())
        {
            auto label = new QLabel(scrollAreaWidgetContents);
            label->setWordWrap(true);
            label->setText(param.notes);
            gridLayout->addWidget(label, row + 1, 1);
        }
    }

    gridLayout->setColumnStretch(1, 1);

    return page;
}

void QnCameraAdvancedParamWidgetsManager::setUpDependenciesForPage(
    const std::vector<QnCameraAdvancedParameter> &params)
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

void QnCameraAdvancedParamWidgetsManager::runAllHandlerChains()
{
    // Run handler chains.
    for (const auto& handlerChains: m_handlerChains)
    {
        for (auto& func: handlerChains)
            func();
    }
}

QnCameraAdvancedParamWidgetsManager::DependencyHandler
QnCameraAdvancedParamWidgetsManager::makeDependencyHandler(
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

QnCameraAdvancedParamWidgetsManager::HandlerChainLauncher
QnCameraAdvancedParamWidgetsManager::makeHandlerChainLauncher(
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

void QnCameraAdvancedParamWidgetsManager::setLabelText(
    QLabel* label,
    const QnCameraAdvancedParameter& parameter,
    const QString& range) const
{
    NX_ASSERT(label);
    if (!label)
        return;

    if (!parameter.showRange)
    {
        label->setText(lit("%1").arg(parameter.name));
        return;
    }

    const auto rangeSplit = range.split(L',');
    if (rangeSplit.size() != 2)
        return;

    NX_ASSERT(parameter.dataType == QnCameraAdvancedParameter::DataType::Number);
    label->setText(
        lit("%1 (%2-%3)")
        .arg(parameter.name)
        .arg(rangeSplit[0].trimmed())
        .arg(rangeSplit[1].trimmed()));
}
