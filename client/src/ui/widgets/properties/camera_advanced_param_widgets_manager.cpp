#include "camera_advanced_param_widgets_manager.h"

QnCameraAdvancedParamWidgetsManager::QnCameraAdvancedParamWidgetsManager(QTreeWidget* groupWidget, QStackedWidget* contentsWidget, QObject* parent /*= NULL*/):
	QObject(parent),
	m_groupWidget(groupWidget),
	m_contentsWidget(contentsWidget)
{

}

void QnCameraAdvancedParamWidgetsManager::displayParams(const QnCameraAdvancedParams &params) {
	disconnect(m_groupWidget, NULL, this, NULL);

	m_groupWidget->clear();
	while (m_contentsWidget->count() > 0)
		m_contentsWidget->removeWidget(m_contentsWidget->widget(0));

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
	QString title = params.empty() ? lit("empty params") : params.back().name;
	return new QLabel(title, m_contentsWidget);
}
