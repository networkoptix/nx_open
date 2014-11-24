#include "camera_advanced_param.h"

#include <boost/range.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/range/algorithm/find_if.hpp>

#include <core/resource/resource.h>
#include <core/resource/param.h>

#include <utils/common/model_functions.h>

namespace {
	const QString defaultXmlFilename = lit(":/camera_settings/camera_settings.xml");
}

namespace QnXmlTag {
	const QString root				= lit("cameras");
	const QString cameraType		= lit("camera");
	const QString cameraTypeName	= lit("name");
	const QString cameraTypeParent	= lit("parent");
	const QString group				= lit("group");
	const QString param				= lit("param");
	const QString name 				= lit("name");
	const QString description		= lit("description");
	const QString query				= lit("query");
	const QString dataType			= lit("dataType");
	const QString method			= lit("method");
	const QString min				= lit("min");
	const QString max				= lit("max");
	const QString step				= lit("step");
	const QString readOnly			= lit("readOnly");
}

QnCameraAdvancedParameter::QnCameraAdvancedParameter():
    dataType(DataType::None),
    readOnly(false)
{

}

QString QnCameraAdvancedParameter::getId() const {
	return query;
}

bool QnCameraAdvancedParameter::isValid() const {
	return !getId().isEmpty();
}

QString QnCameraAdvancedParameter::dataTypeToString(DataType value) {
	switch (value) {
	case QnCameraAdvancedParameter::DataType::Bool:
		return lit("Bool");
	case QnCameraAdvancedParameter::DataType::MinMaxStep:
		return lit("MinMaxStep");
	case QnCameraAdvancedParameter::DataType::Enumeration:
		return lit("Enumeration");
	case QnCameraAdvancedParameter::DataType::Button:
		return lit("Button");
	case QnCameraAdvancedParameter::DataType::String:
		return lit("String");
	case QnCameraAdvancedParameter::DataType::ControlButtonsPair:
		return lit("ControlButtonsPair");
	default:
		return QString();
	}
}

QnCameraAdvancedParameter::DataType QnCameraAdvancedParameter::stringToDataType(const QString &value) {
	static QList<QnCameraAdvancedParameter::DataType> allDataTypes;
	if (allDataTypes.isEmpty())
		allDataTypes 
		<< DataType::Bool 
		<< DataType::MinMaxStep
		<< DataType::Enumeration
		<< DataType::Button
		<< DataType::String
		<< DataType::ControlButtonsPair;
	for (auto dataType: allDataTypes)
		if (dataTypeToString(dataType) == value)
			return dataType;
	return DataType::None;
}

void QnCameraAdvancedParamGroup::merge(const QnCameraAdvancedParamGroup &other) {
	for (const QnCameraAdvancedParamGroup &group: other.groups) {
		QString groupName = group.name;

		auto iter = boost::range::find_if(this->groups, [groupName](const QnCameraAdvancedParamGroup &group) {
			return group.name == groupName;
		});
		if (iter == boost::end(this->groups))
			this->groups.push_back(group);
		else
			iter->merge(group);
	}

	for (const QnCameraAdvancedParameter &param: other.params) {
		QString paramName = param.name;

		auto iter = boost::range::find_if(this->params, [paramName](const QnCameraAdvancedParameter &param) {
			return param.name == paramName;
		});
		/* Parameter should be just replaced. */
		if (iter != boost::end(this->params))
			this->params.erase(iter);
		this->params.push_back(param);
	}
}


void QnCameraAdvancedParams::merge(const QnCameraAdvancedParams &other) {
	for (const QnCameraAdvancedParamGroup &group: other.groups) {
		QString groupName = group.name;

		auto iter = boost::range::find_if(this->groups, [groupName](const QnCameraAdvancedParamGroup &group) {
			return group.name == groupName;
		});
		if (iter == boost::end(this->groups))
			this->groups.push_back(group);
		else
			iter->merge(group);
	}

}


bool QnCameraAdvancedParamsTree::isEmpty() const {
	return cameraTypeName.isEmpty() && children.empty();
}

QnCameraAdvancedParams QnCameraAdvancedParamsTree::flatten(const QString &cameraTypeName) const {
	QnCameraAdvancedParams result = this->params;

	if (this->cameraTypeName == cameraTypeName)
		return result;

	for (auto child: children) {
		if (child.containsSubTree(cameraTypeName))
			result.merge(child.flatten(cameraTypeName));
	}
	return result;
}

bool QnCameraAdvancedParamsTree::containsSubTree(const QString &cameraTypeName) const {
	return cameraTypeName.isEmpty() 
		|| cameraTypeName == this->cameraTypeName 
		|| boost::algorithm::any_of(children, [cameraTypeName](const QnCameraAdvancedParamsTree &child){return child.containsSubTree(cameraTypeName);});
}



QnCameraAdvancedParamsReader::QnCameraAdvancedParamsReader() {}

QnCameraAdvancedParams QnCameraAdvancedParamsReader::params(const QnResourcePtr &resource) const {
	Q_ASSERT(resource);

	QnUuid id = resource->getId();

	/* Check if we have already read parameters for this camera. */
	if (m_paramsByCameraId.contains(id))
		return m_paramsByCameraId[id];

	/* Check if the camera has its own xml. */
	QString cameraSettingsXml = resource->getProperty( Qn::PHYSICAL_CAMERA_SETTINGS_XML_PARAM_NAME );
	/* If yes, read it. */
	if( !cameraSettingsXml.isEmpty() ) {
		QBuffer dataSource;
		dataSource.setData( cameraSettingsXml.toUtf8() );
		QnCameraAdvancedParamsTree params = readXml(&dataSource);
		QnCameraAdvancedParams result = params.flatten();

		/* Cache result for future use. */
		m_paramsByCameraId[id] = result;
		return result;
	} 

	/* Check if we have read default xml. */
	if (m_defaultParamsTree.isEmpty()) {
		/* If not, read it. */
		QFile dataSource(defaultXmlFilename);
		Q_ASSERT_X(dataSource.exists(), Q_FUNC_INFO, "Could not read " + defaultXmlFilename.toLatin1());
		if (dataSource.exists())
			m_defaultParamsTree = readXml(&dataSource);
		Q_ASSERT_X(!m_defaultParamsTree.isEmpty(), Q_FUNC_INFO, defaultXmlFilename.toLatin1() + " is not valid");
	}

	const QString cameraType = calculateCameraType(resource);
	if (cameraType.isEmpty())
		return QnCameraAdvancedParams();

	QnCameraAdvancedParams result = m_defaultParamsTree.flatten(cameraType);
	/* Cache result for future use. */
	m_paramsByCameraId[id] = result;
	return result;
}

QString QnCameraAdvancedParamsReader::calculateCameraType(const QnResourcePtr &resource) const {
	return resource->getProperty( Qn::CAMERA_SETTINGS_ID_PARAM_NAME );
}

void QnCameraAdvancedParamsReader::buildTree(const QMultiMap<QString, QnCameraAdvancedParamsTree> &source, QnCameraAdvancedParamsTree &out) const {
	auto range = source.values(out.cameraTypeName);
	for (QnCameraAdvancedParamsTree& child: range) {
		buildTree(source, child);
		out.children.push_back(child);
	}
}

QnCameraAdvancedParamsTree QnCameraAdvancedParamsReader::readXml(QIODevice *xmlSource) const {
	QnCameraAdvancedParamsTree result;

	QDomDocument xmlDom;
	{
		QString errorStr;
		int errorLine;
		int errorColumn;
		if (!xmlDom.setContent(xmlSource, &errorStr, &errorLine, &errorColumn))
		{
			qWarning() << "Parse xml error at line: " << errorLine << ", column: " << errorColumn << ", error: " << errorStr;
			return result;

		}
	}

	QDomElement root = xmlDom.documentElement();
	if (root.tagName() != QnXmlTag::root) {
		qWarning() << "Parse xml error: could not find 'cameras' tag";
		return result;
	}

	QMultiMap<QString, QnCameraAdvancedParamsTree> treesByParentId;

	for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
		if (node.toElement().tagName() != QnXmlTag::cameraType)
			continue;

		QnCameraAdvancedParamsTree cameraTree;
		cameraTree.cameraTypeName = node.attributes().namedItem(QnXmlTag::cameraTypeName).nodeValue();
		cameraTree.params = parseCameraXml(node.toElement());
		
		QString parentId = node.attributes().namedItem(QnXmlTag::cameraTypeParent).nodeValue();
		/* Delay elements appending until all tree will be read. */
		treesByParentId.insert(parentId, cameraTree);
	}

	buildTree(treesByParentId, result);
	return result;
}

QnCameraAdvancedParams QnCameraAdvancedParamsReader::parseCameraXml(const QDomElement& cameraXml) const {
	QnCameraAdvancedParams result;
	for (QDomNode node = cameraXml.firstChild(); !node.isNull(); node = node.nextSibling()) {
		if (node.nodeName() != QnXmlTag::group) 
			continue;
		
		QnCameraAdvancedParamGroup group = parseGroupXml(node.toElement());
		result.groups.push_back(group);
	}
	return result;
}

QnCameraAdvancedParamGroup QnCameraAdvancedParamsReader::parseGroupXml(const QDomElement &groupXml) const {
	QnCameraAdvancedParamGroup result;

	result.name = groupXml.attribute(QnXmlTag::name);
	result.description = groupXml.attribute(QnXmlTag::description);

	for (QDomNode node = groupXml.firstChild(); !node.isNull(); node = node.nextSibling()) {
		if (node.nodeName() == QnXmlTag::group) {
			QnCameraAdvancedParamGroup group = parseGroupXml(node.toElement());
			result.groups.push_back(group);
		} else if (node.nodeName() == QnXmlTag::param) {
			QnCameraAdvancedParameter param = parseElementXml(node.toElement());
			result.params.push_back(param);
		}
	}
	return result;
}

QnCameraAdvancedParameter QnCameraAdvancedParamsReader::parseElementXml(const QDomElement &elementXml) const {
	QnCameraAdvancedParameter result;

	result.name = elementXml.attribute(QnXmlTag::name);
	result.description = elementXml.attribute(QnXmlTag::description);
	result.query = elementXml.attribute(QnXmlTag::query);
	result.dataType = QnCameraAdvancedParameter::stringToDataType(elementXml.attribute(QnXmlTag::dataType));
	result.method = elementXml.attribute(QnXmlTag::method);
	result.min = elementXml.attribute(QnXmlTag::min);
	result.max = elementXml.attribute(QnXmlTag::max);
	result.step = elementXml.attribute(QnXmlTag::step);
	result.readOnly = elementXml.attribute(QnXmlTag::readOnly) == lit("true");

	return result;
}


bool deserialize(QnJsonContext *, const QJsonValue &value, QnCameraAdvancedParameter::DataType *target) {
	*target = QnCameraAdvancedParameter::stringToDataType(value.toString());
	return true;
}

void serialize(QnJsonContext *ctx, const QnCameraAdvancedParameter::DataType &value, QJsonValue *target) {
	QJson::serialize(ctx, QnCameraAdvancedParameter::dataTypeToString(value), target);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
	QnCameraAdvancedParameterTypes,
	(json),
	_Fields,
	(optional, true)
	)
