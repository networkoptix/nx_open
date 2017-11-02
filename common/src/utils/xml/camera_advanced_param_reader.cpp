#include "camera_advanced_param_reader.h"

#include <QtXml/QDomElement>
#include <QtXmlPatterns/QXmlSchema>
#include <QtXmlPatterns/QXmlSchemaValidator>
#include <QtXmlPatterns/QAbstractMessageHandler>

#include <core/resource/resource.h>
#include <core/resource/param.h>

#include <nx/utils/log/log.h>
#include <nx/fusion/model_functions.h>

namespace {
    const QUrl validatorSchemaUrl = lit("qrc:/camera_advanced_params/schema.xsd");

    //!Prints XML validation errors to log
    class QnXMLValidationMessageHandler: public QAbstractMessageHandler {
    public:
        //!Implementation of QAbstractMessageHandler::handleMessage
        virtual void handleMessage(
            QtMsgType type,
            const QString& description,
            const QUrl& identifier,
            const QSourceLocation& sourceLocation ) override {
                if( type >= QtCriticalMsg ) {
                    NX_LOG( lit("Camera parameters XML validation error. Identifier: %1. Description: %2. Location: %3:%4").
                        arg(identifier.toString()).arg(description).arg(sourceLocation.line()).arg(sourceLocation.column()), cl_logDEBUG1 );
                }
        }
    };

    bool parseBooleanXmlValue(const QString &value) {
        return (value == lit("true")) || (value == lit("1"));
    }

    class QnIODeviceRAAI {
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

    const QString advancedParametersKey(Qn::CAMERA_ADVANCED_PARAMETERS);
}

QnCameraAdvancedParams QnCameraAdvancedParamsReader::paramsFromResource(const QnResourcePtr &resource) {
    NX_ASSERT(resource);
    QByteArray serialized = encodedParamsFromResource(resource).toUtf8();
    if (serialized.isEmpty())
        return QnCameraAdvancedParams();
    return QJson::deserialized<QnCameraAdvancedParams>(serialized);
}

void QnCameraAdvancedParamsReader::setParamsToResource(const QnResourcePtr &resource, const QnCameraAdvancedParams &params) {
    NX_ASSERT(resource);
    QByteArray serialized = QJson::serialized(params);
    setEncodedParamsToResource(resource, QString::fromUtf8(serialized));
}

QString QnCameraAdvancedParamsReader::encodedParamsFromResource(const QnResourcePtr &resource) {
    NX_ASSERT(resource);
    return resource->getProperty(advancedParametersKey);
}

void QnCameraAdvancedParamsReader::setEncodedParamsToResource(const QnResourcePtr &resource, const QString &params) {
    NX_ASSERT(resource);
    resource->setProperty(advancedParametersKey, params);
}

QnCachingCameraAdvancedParamsReader::QnCachingCameraAdvancedParamsReader(QObject* parent):
    base_type(parent)
{}

QnCameraAdvancedParams QnCachingCameraAdvancedParamsReader::params(const QnResourcePtr &resource) const {
    NX_ASSERT(resource);
    QnUuid id = resource->getId();

    /* Check if we have already read parameters for this camera. */
    if (!m_paramsByCameraId.contains(id)) {
        QnCameraAdvancedParams result = paramsFromResource(resource);
        m_paramsByCameraId[id] = result;

        connect(resource, &QnResource::propertyChanged, this, [this](const QnResourcePtr &res, const QString &key) {
            if (key != advancedParametersKey)
                return;
            m_paramsByCameraId.remove(res->getId());
        });

    }
    return m_paramsByCameraId[id];
}

void QnCachingCameraAdvancedParamsReader::setParams(const QnResourcePtr &resource, const QnCameraAdvancedParams &params) const {
    setParamsToResource(resource, params);
    m_paramsByCameraId[resource->getId()] = params;
}

void QnCachingCameraAdvancedParamsReader::clearResourceParams(const QnResourcePtr& resource)
{
    NX_ASSERT(resource);
    m_paramsByCameraId.remove(resource->getId());
}

bool QnCameraAdvacedParamsXmlParser::validateXml(QIODevice *xmlSource)
{
    // TODO: #GDM Why the file is not reset to initial position? It leads to 'EOF' error in parsing.
    return true;

    QnIODeviceRAAI guard(xmlSource);
    if (!guard.isValid())
        return false;

    QXmlSchema schema;
    if (!schema.load(validatorSchemaUrl))
        return false;
    NX_ASSERT(schema.isValid());
    QXmlSchemaValidator validator(schema);
    QnXMLValidationMessageHandler msgHandler;
    validator.setMessageHandler(&msgHandler);
    return validator.validate(xmlSource);
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
    const QString paramRange            = lit("range");
    const QString paramInternalRange    = lit("internalRange");
    const QString paramTag              = lit("tag");
    const QString paramReadOnly         = lit("readOnly");
    const QString paramReadCmd          = lit("readCmd");
    const QString paramWriteCmd         = lit("writeCmd");
    const QString paramAux              = lit("aux");
    const QString paramShowRange        = lit("showRange");
    const QString paramNotes            = lit("notes");
    const QString paramUnit             = lit("unit");
    const QString paramResync           = lit("resync");

    const QString dependenciesRoot      = lit("dependencies");
    const QString dependenciesShow      = lit("dependencies-ranges");
    const QString conditionalShow       = lit("conditional-show");
    const QString dependenciesRanges    = lit("dependencies-ranges");
    const QString conditionalRange      = lit("conditional-range");

    const QString dependencyId          = lit("id");
    const QString conditionId           = lit("id");
    const QString conditionWatch        = lit("watch");
    const QString condition             = lit("condition");
}

bool QnCameraAdvacedParamsXmlParser::readXml(QIODevice *xmlSource, QnCameraAdvancedParams &result) {

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
        NX_LOG(lit("Parse xml error: could not find tag %1. Got %2 instead.")
            .arg(QnXmlTag::plugin)
            .arg(root.tagName()), cl_logWARNING);
		return false;
	}

    result.name         = root.attribute(QnXmlTag::pluginName);
    result.version      = root.attribute(QnXmlTag::pluginVersion);
    result.unique_id    = root.attribute(QnXmlTag::pluginUniqueId);
    result.packet_mode  = parseBooleanXmlValue(root.attribute(QnXmlTag::pluginPacketMode, lit("true")));

	return parsePluginXml(root, result);
}

bool QnCameraAdvacedParamsXmlParser::parsePluginXml(const QDomElement& pluginXml, QnCameraAdvancedParams &params) {
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

bool QnCameraAdvacedParamsXmlParser::parseGroupXml(const QDomElement &groupXml, QnCameraAdvancedParamGroup &group) {

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

bool QnCameraAdvacedParamsXmlParser::parseElementXml(const QDomElement& elementXml,
    QnCameraAdvancedParameter& param)
{
    param.id = elementXml.attribute(QnXmlTag::paramId);
    param.dataType = QnCameraAdvancedParameter::stringToDataType(elementXml.attribute(QnXmlTag::paramDataType));
    param.name = elementXml.attribute(QnXmlTag::paramName);
    param.description = elementXml.attribute(QnXmlTag::paramDescription);
    param.confirmation = elementXml.attribute(QnXmlTag::paramConfirmation);
    param.range = elementXml.attribute(QnXmlTag::paramRange);
    param.internalRange = elementXml.attribute(QnXmlTag::paramInternalRange);
    param.tag = elementXml.attribute(QnXmlTag::paramTag);
    param.readOnly = parseBooleanXmlValue(elementXml.attribute(QnXmlTag::paramReadOnly));
    param.readCmd = elementXml.attribute(QnXmlTag::paramReadCmd);
    param.writeCmd = elementXml.attribute(QnXmlTag::paramWriteCmd);
    param.aux = elementXml.attribute(QnXmlTag::paramAux);
    param.showRange = parseBooleanXmlValue(elementXml.attribute(QnXmlTag::paramShowRange));
    param.notes = elementXml.attribute(QnXmlTag::paramNotes);
    param.unit = elementXml.attribute(QnXmlTag::paramUnit);
    param.resync = parseBooleanXmlValue(elementXml.attribute(QnXmlTag::paramResync));

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

bool QnCameraAdvacedParamsXmlParser::parseDependencyGroupsXml(
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

bool QnCameraAdvacedParamsXmlParser::parseDependenciesXml(
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
            dependency.type = QnCameraAdvancedParameterDependency::DependencyType::Show;
        }
        else if (depNode.nodeName() == QnXmlTag::conditionalRange)
        {
            dependency.type = QnCameraAdvancedParameterDependency::DependencyType::Range;
            dependency.range = depNode.attribute(QnXmlTag::paramRange);
            dependency.internalRange = depNode.attribute(QnXmlTag::paramInternalRange);
        }

        dependency.id = depNode.attribute(QnXmlTag::dependencyId);

        isOk = parseConditionsXml(depNode, dependency.conditions);

        if (!isOk)
            break;

        dependencies.push_back(dependency);
    }

    return isOk;
}

bool QnCameraAdvacedParamsXmlParser::parseConditionsXml(
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

bool QnCameraAdvacedParamsXmlParser::parseConditionString(
    QString conditionString,
    QnCameraAdvancedParameterCondition &condition)
{
    if (conditionString.isEmpty())
        return false;

    const auto split = conditionString.split(L'=');
    if (split.isEmpty())
        return false;

    auto condTypeStr = split[0].trimmed();
    auto condValueStr = split.size() > 1
        ? split[1].trimmed()
        : QString();

    condition.type = QnCameraAdvancedParameterCondition::fromStringToConditionType(condTypeStr);

    if (condition.type == QnCameraAdvancedParameterCondition::ConditionType::Unknown)
        return false;

    condition.value = condValueStr;

    return true;
}
