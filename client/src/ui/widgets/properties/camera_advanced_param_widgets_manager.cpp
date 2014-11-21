#include "camera_advanced_param_widgets_manager.h"

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

void QnCameraAdvancedParamWidgetsManager::loadValues(const QHash<QString, QString> &valuesById) {
	for (auto iter = m_paramWidgetsById.cbegin(); iter != m_paramWidgetsById.cend(); ++iter) {
		QString id = iter.key();
		if (!valuesById.contains(id))
			continue;

		auto widget = iter.value();
		widget->setValue(valuesById[id]);
		widget->setEnabled(true);
		connect(widget, &QnAbstractCameraAdvancedParamWidget::valueChanged, this, &QnCameraAdvancedParamWidgetsManager::paramValueChanged);
	}
}


void QnCameraAdvancedParamWidgetsManager::createGroupWidgets(const QnCameraAdvancedParamGroup &group, QTreeWidgetItem* parentItem) {
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

	QWidget* contentsPage = createContentsPage(group.params);
	m_contentsWidget->addWidget(contentsPage);

	item->setData(0, Qt::UserRole, qVariantFromValue(contentsPage));

	for (const QnCameraAdvancedParamGroup &group: group.groups)
		createGroupWidgets(group, item);
}


QWidget* QnCameraAdvancedParamWidgetsManager::createContentsPage(const std::vector<QnCameraAdvancedParameter> &params) {

	QScrollArea* listWidget = new QScrollArea(m_contentsWidget);
	listWidget->setWidgetResizable(true);
	
	QFormLayout* formLayout = new QFormLayout(listWidget);
	listWidget->setLayout(formLayout);

	for(const QnCameraAdvancedParameter &param: params) {
		QnAbstractCameraAdvancedParamWidget* widget = QnCameraAdvancedParamWidgetFactory::createWidget(param, listWidget);
		if (!widget)
			continue;

		if (param.dataType != QnCameraAdvancedParameter::DataType::Button)
			formLayout->addRow(param.name, widget);
		else
			formLayout->addRow(QString(), widget);

		if (!param.getId().isEmpty()) {
			m_paramWidgetsById[param.getId()] = widget;
			/* Widget is disabled until it receive correct value. */
			widget->setEnabled(false);
		}
	}

	//listLayout->addStretch();

	return listWidget;
}
