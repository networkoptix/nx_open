#include "camera_advanced_param_widgets_manager.h"

#include <boost/range.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include <ui/widgets/properties/camera_advanced_param_widget_factory.h>

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
	QGroupBox* groupBox = new QGroupBox(m_contentsWidget);
    groupBox->setFlat(true);
    groupBox->setTitle(name);
	groupBox->setLayout(new QHBoxLayout(groupBox));

	QScrollArea* scrollArea = new QScrollArea(groupBox);
	scrollArea->setWidgetResizable(true);
	groupBox->layout()->addWidget(scrollArea);

	QWidget* scrollAreaWidgetContents = new QWidget();
    scrollAreaWidgetContents->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);

	QGridLayout* gridLayout = new QGridLayout(scrollAreaWidgetContents);
	scrollAreaWidgetContents->setLayout(gridLayout);
    scrollArea->setWidget(scrollAreaWidgetContents);

    for (const auto& param : params)
    {
        QnAbstractCameraAdvancedParamWidget* widget = QnCameraAdvancedParamWidgetFactory::createWidget(param, scrollAreaWidgetContents);
        if (!widget)
            continue;

        if (param.dataType != QnCameraAdvancedParameter::DataType::Button)
        {
            QLabel* label = new QLabel(scrollAreaWidgetContents);
            label->setToolTip(param.description);
            label->setText(lit("%1: ").arg(param.name));
            label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            label->setBuddy(widget);
            gridLayout->addWidget(label, gridLayout->rowCount(), 0);
        }

        gridLayout->addWidget(widget, gridLayout->rowCount(), 1);
		m_paramWidgetsById[param.id] = widget;

		/* Widget is disabled until it receives correct value. */
		if (QnCameraAdvancedParameter::dataTypeHasValue(param.dataType))
			widget->setEnabled(false);
        else
            connect(widget, &QnAbstractCameraAdvancedParamWidget::valueChanged, this, &QnCameraAdvancedParamWidgetsManager::paramValueChanged);
	}

	return groupBox;
}
