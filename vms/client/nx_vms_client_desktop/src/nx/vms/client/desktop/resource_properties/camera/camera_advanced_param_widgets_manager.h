#pragma once

#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QLabel>

#include <nx/utils/std/optional.h>
#include <nx/utils/scoped_connections.h>

#include <core/resource/camera_advanced_param.h>

namespace nx::vms::client::desktop {

class AbstractCameraAdvancedParamWidget;

class CameraAdvancedParamWidgetsManager: public QObject
{
    Q_OBJECT

public:
    explicit CameraAdvancedParamWidgetsManager(
        QTreeWidget* groupWidget,
        QStackedWidget* contentsWidget,
        QObject* parent = NULL);

    void displayParams(const QnCameraAdvancedParams &params);

    /**
     * @param params List of new parameter values.
     * @param packetMode If true then all the parameter widgets will be disabled and reenabled
     *    back only if the parameter is in the params list. Otherwise only parameter values will
     *    be updated.
     *
     *    This parameter is needed for some cameras that include/exclude some parameter
     *    values in the response depending on the other parameter states. Some cameras also
     *    permit to change some parameters only if their dependencies are in a certain state.
     *    Those parameters can implicitly change their values or implicitly become
     *    enabled/disabled. Because of that, server has to send all the values of all the
     *    parameters regardless of the parameter requested to change.
     *
     *    It's a pretty expensive opertaion, so it's enabled only for certain cameras. Packet mode
     *    is quite similar to the `resync` property of QnCameraAdvancedParameter, the difference
     *    is `resync` is applied to particular parameters and requires additional request to the
     *    server while packet mode implies that the server always sends all the parameter values
     *    in a response to the save request, so parameters that are absent in the response must be
     *    considered disabled.
     */
    void loadValues(const QnCameraAdvancedParamValueList &params, bool packetMode = false);
    std::optional<QString> parameterValue(const QString& parameterId) const;

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
    QHash<QString, AbstractCameraAdvancedParamWidget*> m_paramWidgetsById;
    QHash<QString, QWidget*> m_paramLabelsById;
    QMap<QString, QVector<std::function<void()>>> m_handlerChains;
    QHash<QString, QnCameraAdvancedParameter> m_parametersById;
    nx::utils::ScopedConnections m_handlerChainConnections;
};

} // namespace nx::vms::client::desktop
