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

void QnCameraAdvancedParamWidgetsManager::clear() {
	disconnect(m_groupWidget, NULL, this, NULL);

	for(QnAbstractCameraAdvancedParamWidget* widget: m_paramWidgetsById)
		disconnect(widget, NULL, this, NULL);

	m_paramWidgetsById.clear();
    m_paramLabelsById.clear();
	m_groupWidget->clear();
	while (m_contentsWidget->count() > 0)
		m_contentsWidget->removeWidget(m_contentsWidget->widget(0));
}


void QnCameraAdvancedParamWidgetsManager::displayParams(const QnCameraAdvancedParams& params)
{
	clear();

	for (const QnCameraAdvancedParamGroup &group: params.groups)
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

void QnCameraAdvancedParamWidgetsManager::loadValues(const QnCameraAdvancedParamValueList &params) {
	for (const QnCameraAdvancedParamValue &param: params) {
		if (!m_paramWidgetsById.contains(param.id))
			continue;

		auto widget = m_paramWidgetsById[param.id];
		widget->setValue(param.value);
		widget->setEnabled(true);
		connect(widget, &QnAbstractCameraAdvancedParamWidget::valueChanged, this, &QnCameraAdvancedParamWidgetsManager::paramValueChanged);
	}
}

bool QnCameraAdvancedParamWidgetsManager::hasValidValues(const QnCameraAdvancedParamGroup &group) const {
    bool hasValidParameter = boost::algorithm::any_of(group.params, [](const QnCameraAdvancedParameter &param){return param.isValid();});
	if (hasValidParameter)
		return true;
    return boost::algorithm::any_of(group.groups, [this](const QnCameraAdvancedParamGroup &group){return hasValidValues(group);});
}


void QnCameraAdvancedParamWidgetsManager::createGroupWidgets(const QnCameraAdvancedParamGroup &group, QTreeWidgetItem* parentItem) {
	if (!hasValidValues(group))
		return;

	QTreeWidgetItem* item;
	if (parentItem) {
		item = new QTreeWidgetItem(parentItem);
		parentItem->addChild(item);
	}
	else {
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

    for (const QnCameraAdvancedParamGroup &subGroup: group.groups)
        createGroupWidgets(subGroup, item);
}

QWidget* QnCameraAdvancedParamWidgetsManager::createContentsPage(const QString& name, const std::vector<QnCameraAdvancedParameter>& params)
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

    for (const auto& param: params)
    {
        auto widget = QnCameraAdvancedParamWidgetFactory::createWidget(param, scrollAreaWidgetContents);
        if (!widget)
            continue;

        const auto row = gridLayout->rowCount();

        if (param.dataType == QnCameraAdvancedParameter::DataType::Separator)
        {
            gridLayout->addWidget(widget, row, 0, 1, 2);
            continue;
        }

        if (param.dataType != QnCameraAdvancedParameter::DataType::Button)
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
        }

        gridLayout->addWidget(widget, row, 1, Qt::AlignVCenter);
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

    for (const auto& param: params)
    {
        if (param.dependencies.empty())
            continue;

        std::set<QString> watches;
        std::map<DependencyType, std::vector<std::function<bool()>>> handlerChains;

        for (const auto& dependency: param.dependencies)
        {
            for (const auto& cond: dependency.conditions)
                watches.insert(cond.paramId);

            auto handler =
                [dependency, param, this]() -> bool
                {
                    const auto paramId = param.id;
                    bool allConditionsSatisfied = std::all_of(
                        dependency.conditions.cbegin(), dependency.conditions.cend(),
                        [this](const QnCameraAdvancedParameterCondition& condition)
                        {
                            auto widget = m_paramWidgetsById.value(condition.paramId);
                            return widget && condition.checkValue(widget->value());
                        });

                    // TODO: #dmishin move this somewhere.
                    if (dependency.type == DependencyType::Show)
                    {
                        if (auto label = m_paramLabelsById.value(paramId))
                            label->setHidden(!allConditionsSatisfied);
                        if (auto widget = m_paramWidgetsById.value(paramId))
                            widget->setHidden(!allConditionsSatisfied);
                    }
                    else if (dependency.type == DependencyType::Range)
                    {
                        if (!allConditionsSatisfied)
                            return false;

                        auto widget = m_paramWidgetsById.value(paramId);
                        if (!widget)
                            return false;

                        widget->setRange(dependency.range);

                        auto label = dynamic_cast<QLabel*>(m_paramLabelsById[paramId]);
                        if (!label)
                            return false;

                        setLabelText(label, param, dependency.range);
                    }

                    return allConditionsSatisfied;
                };

            handlerChains[dependency.type].push_back(handler);
        }

        auto runHandlerChains =
            [handlerChains]()
            {
                for (const auto& chainPair: handlerChains)
                {
                    auto& chain = chainPair.second;

                    for (const auto& handler: chain)
                    {
                        if (handler())
                            break;
                    }
                }
            };

        for (const auto& watch: watches)
        {
            if (auto widget = m_paramWidgetsById.value(watch))
            {
                connect(widget, &QnAbstractCameraAdvancedParamWidget::valueChanged,
                    runHandlerChains);

                runHandlerChains();
            }
        }
    }

	return page;
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
