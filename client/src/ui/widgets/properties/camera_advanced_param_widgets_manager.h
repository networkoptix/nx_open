#ifndef QN_CAMERA_ADVANCED_PARAM_WIDGETS_MANAGER_H
#define QN_CAMERA_ADVANCED_PARAM_WIDGETS_MANAGER_H

#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QStackedWidget>

#include <core/resource/camera_advanced_param.h>

class QnAbstractCameraAdvancedParamWidget;

class QnCameraAdvancedParamWidgetsManager: public QObject {
	Q_OBJECT
public:
	explicit QnCameraAdvancedParamWidgetsManager(QTreeWidget* groupWidget, QStackedWidget* contentsWidget, QObject* parent = NULL);

	void displayParams(const QnCameraAdvancedParams &params);
	void loadValues(const QHash<QString, QString> &valuesById);

signals:
	void paramValueChanged(const QString &id, const QString &value);

private:
	void clear();

	/** Check if group has at least one valid value. */
	bool hasValidValues(const QnCameraAdvancedParamGroup &group) const;

	void createGroupWidgets(const QnCameraAdvancedParamGroup &group, QTreeWidgetItem* parentItem = NULL);
	QWidget* createContentsPage(const QString &name, const std::vector<QnCameraAdvancedParameter> &params);

private:
	QTreeWidget* m_groupWidget;
	QStackedWidget* m_contentsWidget;
	QHash<QString, QnAbstractCameraAdvancedParamWidget*> m_paramWidgetsById;
};

#endif //QN_CAMERA_ADVANCED_PARAM_WIDGETS_MANAGER_H