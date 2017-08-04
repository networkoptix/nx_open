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


void QnCameraAdvancedParamWidgetsManager::displayParams(const QnCameraAdvancedParams &params) {
	clear();

	for (const QnCameraAdvancedParamGroup &group: params.groups)
		createGroupWidgets(group);

	connect(m_groupWidget, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current, QTreeWidgetItem *previous) {
		Q_UNUSED(previous);
		QWidget* target = current->data(0, Qt::UserRole).value<QWidget*>();
		if (target && m_contentsWidget->children().contains(target))
			m_contentsWidget->setCurrentWidget(target);
	});

	if (m_groupWidget->topLevelItemCount() > 0)
		m_groupWidget->setCurrentItem(m_groupWidget->topLevelItem(0));
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

	QWidget* contentsPage = createContentsPage(group.name, group.params);
	m_contentsWidget->addWidget(contentsPage);

	item->setData(0, Qt::UserRole, qVariantFromValue(contentsPage));

    for (const QnCameraAdvancedParamGroup &subGroup: group.groups)
        createGroupWidgets(subGroup, item);
}

QWidget* QnCameraAdvancedParamWidgetsManager::createContentsPage(const QString& name, const std::vector<QnCameraAdvancedParameter>& params)
{
    auto page = new QWidget(m_contentsWidget);
    auto pageLayout = new QHBoxLayout(page);
    pageLayout->setContentsMargins(style::Metrics::kDefaultTopLevelMargin, 0, 0, 0);

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
	scrollAreaWidgetContents->setLayout(gridLayout);
    scrollArea->setWidget(scrollAreaWidgetContents);

    for (const auto& param : params)
    {
        auto widget = QnCameraAdvancedParamWidgetFactory::createWidget(param, scrollAreaWidgetContents);
        if (!widget)
            continue;

        int row = gridLayout->rowCount();
        if (param.dataType != QnCameraAdvancedParameter::DataType::Button)
        {
            auto label = new QLabel(scrollAreaWidgetContents);
            label->setToolTip(param.description);
            label->setText(lit("%1: ").arg(param.name));
            label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            label->setBuddy(widget);
            gridLayout->addWidget(label, row, 0);
            m_paramLabelsById[param.id] = label;
        }

        gridLayout->addWidget(widget, row, 1, Qt::AlignLeft | Qt::AlignVCenter);
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

            auto handler = [dependency, paramId = param.id, this]() -> bool
                {
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
                        if (auto widget = m_paramWidgetsById.value(paramId))
                        {
                            if (allConditionsSatisfied)
                                m_paramWidgetsById[paramId]->setRange(dependency.range);
                        }
                    }

                    return allConditionsSatisfied;
                };

            handlerChains[dependency.type].push_back(handler);
        }

        auto runHandlerChains = [handlerChains]()
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
