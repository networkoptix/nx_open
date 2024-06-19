// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_advanced_param_reader.h"

#include <QtCore/QIODevice>
#include <QtXml/QDomElement>

#include <nx/utils/log/log.h>
#include <nx/reflect/string_conversion.h>

namespace
{
    bool parseBooleanXmlValue(const QString &value)
    {
        return (value == lit("true")) || (value == lit("1"));
    }

    class QnIODeviceRAAI
    {
    public:
        QnIODeviceRAAI(QIODevice *device):
            m_device(device)
        {
                m_valid = device->open(QIODevice::ReadOnly);
                if (m_valid)
                    device->reset();
        }

        ~QnIODeviceRAAI() {
            if (m_valid)
                m_device->close();
        }

        bool isValid() const {
            return m_valid;
        }
    private:
        QIODevice* m_device;
        bool m_valid;
    };

    QStringList splitAndTrim(const QString& str, const QChar& separator)
    {
        QStringList result;
        const auto values = str.split(separator);

        for (const auto& value: values)
        {
            const auto trimmed = value.trimmed();
            if (!trimmed.isEmpty())
                result.push_back(trimmed);
        }
        return result;
    }
}

bool QnCameraAdvancedParamsXmlParser::validateXml(QIODevice *xmlSource)
{
    // TODO: #sivanov Why the file is not reset to initial position?
    // It leads to 'EOF' error in parsing.
    return true;
}

namespace QnXmlTag {
    const QString plugin                = lit("plugin");
    const QString pluginName            = lit("name");
    const QString pluginVersion         = lit("version");
    const QString pluginUniqueId        = lit("unique_id");
    const QString pluginPacketMode      = lit("packet_mode");
    const QString parameters            = lit("parameters");
    const QString group                 = lit("group");
    const QString param                 = lit("param");
    const QString groupName             = lit("name");
    const QString groupDescription      = lit("description");
    const QString groupAux              = lit("aux");
    const QString paramId               = lit("id");
    const QString paramDataType         = lit("dataType");
    const QString paramName             = lit("name");
    const QString paramDescription      = lit("description");
    const QString paramConfirmation     = lit("confirmation");
    const QString paramActionName       = lit("actionName");
    const QString paramRange            = lit("range");
    const QString paramInternalRange    = lit("internalRange");
    const QString paramTag              = lit("tag");
    const QString paramReadOnly         = lit("readOnly");
    const QString paramReadCmd          = lit("readCmd");
    const QString paramWriteCmd         = lit("writeCmd");
    const QString paramAux              = lit("aux");
    const QString paramShowRange        = lit("showRange");
    const QString paramCompact          = lit("compact");
    const QString paramNotes            = lit("notes");
    const QString paramUnit             = lit("unit");
    const QString paramResync           = lit("resync");
    const QString paramDefaultValue     = lit("defaultValue");
    const QString paramAvailableInOffline = lit("availableInOffline");
    const QString paramShouldKeepInitialValue = lit("keepInitialValue");
    const QString paramBindDefaultToMinimum = lit("bindDefaultToMinimum");
    const QString parameterGroup = lit("group");

    const QString dependenciesRoot          = lit("dependencies");
    const QString dependenciesRanges        = lit("dependencies-ranges");
    const QString dependenciesTrigger       = lit("dependencies-trigger");

    const QString conditionalShow           = lit("conditional-show");
    const QString conditionalRange          = lit("conditional-range");
    const QString conditionalTrigger        = lit("conditional-triggger");

    const QString dependencyId          = lit("id");
    const QString conditionId           = lit("id");
    const QString conditionWatch        = lit("watch");
    const QString condition             = lit("condition");
}

bool QnCameraAdvancedParamsXmlParser::readXml(QIODevice *xmlSource, QnCameraAdvancedParams &result) {

    QnIODeviceRAAI guard(xmlSource);
    if (!guard.isValid())
        return false;

    QDomDocument xmlDom;
    {
        QString errorStr;
        int errorLine;
        int errorColumn;
        if (!xmlDom.setContent(xmlSource, &errorStr, &errorLine, &errorColumn)) {
            qWarning() << "Parse xml error at line: " << errorLine << ", column: " << errorColumn << ", error: " << errorStr;
            return false;
        }
    }

    QDomElement root = xmlDom.documentElement();
    if (root.tagName() != QnXmlTag::plugin) {
        NX_WARNING(typeid(QnCameraAdvancedParamsXmlParser),
            lit("Parse xml error: could not find tag %1. Got %2 instead.")
            .arg(QnXmlTag::plugin)
            .arg(root.tagName()));
        return false;
    }

    result.name = root.attribute(QnXmlTag::pluginName);
    result.version = root.attribute(QnXmlTag::pluginVersion);
    result.pluginUniqueId = root.attribute(QnXmlTag::pluginUniqueId);
    result.packet_mode = parseBooleanXmlValue(root.attribute(QnXmlTag::pluginPacketMode, lit("true")));

    return parsePluginXml(root, result);
}

bool QnCameraAdvancedParamsXmlParser::parsePluginXml(const QDomElement& pluginXml, QnCameraAdvancedParams &params) {
    for (QDomNode node = pluginXml.firstChild(); !node.isNull(); node = node.nextSibling()) {
        if (node.nodeName() != QnXmlTag::parameters)
            continue;

        for (QDomNode groupNode = node.toElement().firstChild(); !groupNode.isNull(); groupNode = groupNode.nextSibling()) {
            QnCameraAdvancedParamGroup group;
            if (!parseGroupXml(groupNode.toElement(), group))
                return false;
            params.groups.push_back(group);
        }
    }
    return true;
}

bool QnCameraAdvancedParamsXmlParser::parseGroupXml(const QDomElement &groupXml, QnCameraAdvancedParamGroup &group) {

    group.name = groupXml.attribute(QnXmlTag::groupName);
    group.description = groupXml.attribute(QnXmlTag::groupDescription);
    group.aux = groupXml.attribute(QnXmlTag::groupAux);

    for (QDomNode node = groupXml.firstChild(); !node.isNull(); node = node.nextSibling()) {
        if (node.nodeName() == QnXmlTag::group) {
            QnCameraAdvancedParamGroup subGroup;
            if (!parseGroupXml(node.toElement(), subGroup))
                return false;
            group.groups.push_back(subGroup);
        } else if (node.nodeName() == QnXmlTag::param) {
            QnCameraAdvancedParameter param;
            if (!parseElementXml(node.toElement(), param))
                return false;
            group.params.push_back(param);
        }
    }
    return true;
}

bool QnCameraAdvancedParamsXmlParser::parseElementXml(const QDomElement& elementXml,
    QnCameraAdvancedParameter& param)
{
    param.id = elementXml.attribute(QnXmlTag::paramId);
    param.dataType = nx::reflect::fromString(
        elementXml.attribute(QnXmlTag::paramDataType).toStdString(),
        QnCameraAdvancedParameter::DataType::None);
    param.name = elementXml.attribute(QnXmlTag::paramName);
    param.description = elementXml.attribute(QnXmlTag::paramDescription);
    param.confirmation = elementXml.attribute(QnXmlTag::paramConfirmation);
    param.actionName = elementXml.attribute(QnXmlTag::paramActionName);
    param.range = elementXml.attribute(QnXmlTag::paramRange);
    param.internalRange = elementXml.attribute(QnXmlTag::paramInternalRange);
    param.tag = elementXml.attribute(QnXmlTag::paramTag);
    param.readOnly = parseBooleanXmlValue(elementXml.attribute(QnXmlTag::paramReadOnly));
    param.readCmd = elementXml.attribute(QnXmlTag::paramReadCmd);
    param.writeCmd = elementXml.attribute(QnXmlTag::paramWriteCmd);
    param.aux = elementXml.attribute(QnXmlTag::paramAux);
    param.showRange = parseBooleanXmlValue(elementXml.attribute(QnXmlTag::paramShowRange));
    param.compact = parseBooleanXmlValue(elementXml.attribute(QnXmlTag::paramCompact));
    param.notes = elementXml.attribute(QnXmlTag::paramNotes);
    param.unit = elementXml.attribute(QnXmlTag::paramUnit);
    param.resync = parseBooleanXmlValue(elementXml.attribute(QnXmlTag::paramResync));
    param.keepInitialValue = parseBooleanXmlValue(
        elementXml.attribute(QnXmlTag::paramShouldKeepInitialValue));
    param.availableInOffline = parseBooleanXmlValue(
        elementXml.attribute(QnXmlTag::paramAvailableInOffline));
    param.bindDefaultToMinimum = parseBooleanXmlValue(
        elementXml.attribute(QnXmlTag::paramBindDefaultToMinimum));
    param.group = elementXml.attribute(QnXmlTag::parameterGroup);

    auto childNodes = elementXml.childNodes();

    if (childNodes.isEmpty())
        return true;

    for (int i = 0; i < childNodes.size(); ++i)
    {
        auto node = childNodes.at(i).toElement();

        if (node.isNull())
            continue;

        if (node.nodeName() == QnXmlTag::dependenciesRoot)
        {
            if (!parseDependencyGroupsXml(node.toElement(), param.dependencies))
                return false;
        }
    }

    return true;
}

bool QnCameraAdvancedParamsXmlParser::parseDependencyGroupsXml(
    const QDomElement& elementXml,
    std::vector<QnCameraAdvancedParameterDependency>& dependencies)
{
    auto dependencyGroupNodes = elementXml.childNodes();

    bool isOk = true;
    for (int i = 0; i < dependencyGroupNodes.size(); ++i)
    {
        auto depGroup = dependencyGroupNodes.at(i).toElement();

        if (depGroup.isNull())
            continue;

        isOk = parseDependenciesXml(depGroup, dependencies);

        if (!isOk)
            break;
    }

    return isOk;
}

bool QnCameraAdvancedParamsXmlParser::parseDependenciesXml(
    const QDomElement& elementXml,
    std::vector<QnCameraAdvancedParameterDependency>& dependencies)
{
    bool isOk = true;
    auto dependencyNodes = elementXml.childNodes();
    for (int i = 0;  i < dependencyNodes.size(); ++i)
    {
        auto depNode = dependencyNodes.at(i).toElement();

        if (depNode.isNull())
            continue;

        QnCameraAdvancedParameterDependency dependency;

        if (depNode.nodeName() == QnXmlTag::conditionalShow)
        {
            dependency.type = QnCameraAdvancedParameterDependency::DependencyType::show;
        }
        else if (depNode.nodeName() == QnXmlTag::conditionalRange)
        {
            dependency.type = QnCameraAdvancedParameterDependency::DependencyType::range;
            dependency.range = depNode.attribute(QnXmlTag::paramRange);
            dependency.internalRange = depNode.attribute(QnXmlTag::paramInternalRange);
            dependency.valuesToAddToRange
                = splitAndTrim(depNode.attribute(lit("add-values-to-range")), ',');
            dependency.valuesToRemoveFromRange
                = splitAndTrim(depNode.attribute(lit("remove-values-from-range")), ',');
        }
        else if (depNode.nodeName() == QnXmlTag::conditionalTrigger)
        {
            dependency.type = QnCameraAdvancedParameterDependency::DependencyType::trigger;
        }

        dependency.id = depNode.attribute(QnXmlTag::dependencyId);

        isOk = parseConditionsXml(depNode, dependency.conditions);

        if (!isOk)
            break;

        dependencies.push_back(dependency);
    }

    return isOk;
}

bool QnCameraAdvancedParamsXmlParser::parseConditionsXml(
    const QDomElement &elementXml,
    std::vector<QnCameraAdvancedParameterCondition> &conditions)
{
    auto childNodes = elementXml.childNodes();

    for (int i = 0; i < childNodes.size(); ++i)
    {
        auto element = childNodes.at(i).toElement();

        if (element.isNull())
            continue;

        QnCameraAdvancedParameterCondition condition;
        condition.paramId = element.attribute(QnXmlTag::conditionWatch);

        auto conditionStr = element.attribute(QnXmlTag::condition);

        if (!parseConditionString(conditionStr, condition))
            return false;

        conditions.push_back(condition);
    }

    return true;
}

bool QnCameraAdvancedParamsXmlParser::parseConditionString(
    QString conditionString,
    QnCameraAdvancedParameterCondition &condition)
{
    if (conditionString.isEmpty())
        return false;

    const auto split = conditionString.split('=');
    if (split.isEmpty())
        return false;

    auto condTypeStr = split[0].trimmed();
    auto condValueStr = split.size() > 1
        ? split[1].trimmed()
        : QString();

    condition.type = nx::reflect::fromString(
        condTypeStr.toStdString(),
        QnCameraAdvancedParameterCondition::ConditionType::unknown);

    if (condition.type == QnCameraAdvancedParameterCondition::ConditionType::unknown)
        return false;

    condition.value = condValueStr;

    return true;
}
