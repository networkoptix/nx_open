#pragma once

#include <boost/optional.hpp>

#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QLabel>

#include <core/resource/camera_advanced_param.h>

class QnAbstractCameraAdvancedParamWidget;

class QnCameraAdvancedParamWidgetsManager: public QObject
{
    Q_OBJECT
public:
    explicit QnCameraAdvancedParamWidgetsManager(
        QTreeWidget* groupWidget,
        QStackedWidget* contentsWidget,
        QObject* parent = NULL);

    void displayParams(const QnCameraAdvancedParams &params);
    void loadValues(const QnCameraAdvancedParamValueList &params);
    boost::optional<QString> parameterValue(const QString& parameterId) const;


    void clear();

signals:
    void paramValueChanged(const QString &id, const QString &value);

private:

    /** Check if group has at least one valid value. */
    bool hasValidValues(const QnCameraAdvancedParamGroup &group) const;

    void createGroupWidgets(const QnCameraAdvancedParamGroup &group, QTreeWidgetItem* parentItem = NULL);
    QWidget* createContentsPage(const QString &name, const std::vector<QnCameraAdvancedParameter> &params);
    QWidget* createWidgetsForPage(const QString &name, const std::vector<QnCameraAdvancedParameter> &params);
    void setUpDependenciesForPage(const std::vector<QnCameraAdvancedParameter> &params);
    void runAllHandlerChains();

    using DependencyHandler = std::function<bool()>;
    using HandlerChains = std::map<
        QnCameraAdvancedParameterDependency::DependencyType,
        std::vector<std::function<bool()>>>;

    using HandlerChainLauncher = std::function<void()>;

    DependencyHandler makeDependencyHandler(
        const QnCameraAdvancedParameterDependency& dependency,
        const QnCameraAdvancedParameter& parameter) const;

    HandlerChainLauncher makeHandlerChainLauncher(const HandlerChains& handlerChains) const;

    void setLabelText(
        QLabel* label,
        const QnCameraAdvancedParameter& parameter,
        const QString& range) const;

private:
    QTreeWidget* m_groupWidget;
    QStackedWidget* m_contentsWidget;
    QHash<QString, QnAbstractCameraAdvancedParamWidget*> m_paramWidgetsById;
    QHash<QString, QWidget*> m_paramLabelsById;
    QMap<QString, QVector<std::function<void()>>> m_handlerChains;
    QHash<QString, QnCameraAdvancedParameter> m_parametersById;
    QVector<QMetaObject::Connection> m_handlerChainConnections;
};
