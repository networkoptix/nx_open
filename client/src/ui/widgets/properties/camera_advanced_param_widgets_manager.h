#ifndef QN_CAMERA_ADVANCED_PARAM_WIDGETS_MANAGER_H
#define QN_CAMERA_ADVANCED_PARAM_WIDGETS_MANAGER_H

#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QStackedWidget>

#include <core/resource/camera_advanced_param.h>

class QnCameraAdvancedParamWidgetsManager: public QObject {
	Q_OBJECT
public:
	explicit QnCameraAdvancedParamWidgetsManager(QTreeWidget* groupWidget, QStackedWidget* contentsWidget, QObject* parent = NULL);

	void displayParams(const QnCameraAdvancedParams &params);

private:
	void createGroupWidgets(const QnCameraAdvancedParamGroup &group, QTreeWidgetItem* parentItem = NULL);
	QWidget* createContentsPage(const std::vector<QnCameraAdvancedParameter> &params);

private:
	QTreeWidget* m_groupWidget;
	QStackedWidget* m_contentsWidget;
};

#endif //QN_CAMERA_ADVANCED_PARAM_WIDGETS_MANAGER_H