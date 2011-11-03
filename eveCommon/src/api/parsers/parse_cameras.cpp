#include "api/parsers/parse_cameras.h"

#include "core/resourcemanagment/asynch_seacher.h"

void parseCameras(QList<QnResourcePtr>& cameras, const QnApiCameras& xsdCameras, QnResourceFactory& resourceFactory)
{
    using xsd::api::cameras::Cameras;
    using xsd::api::resources::Properties;
    using xsd::api::resourceTypes::ParentIDs;
    using xsd::api::resourceTypes::PropertyTypes;
    using xsd::api::resourceTypes::ParamType;

    for (Cameras::camera_const_iterator i (xsdCameras.begin()); i != xsdCameras.end(); ++i)
    {
        QnResourceParameters parameters;
        parameters["id"] = i->id().c_str();
        parameters["name"] = i->name().c_str();
        parameters["status"] = i->status().c_str();
        parameters["url"] = i->url().c_str();
        parameters["mac"] = i->mac().c_str();
        parameters["login"] = i->login().c_str();
        parameters["password"] = i->password().c_str();

        QnResourcePtr camera = resourceFactory.createResource(i->typeId().c_str(), parameters);
        QnParamList& paramList = camera->getResourceParamList();

        if (i->properties().present())
        {
            const Properties& properties = *(i->properties());
            for (Properties::property_const_iterator j(properties.property().begin()); j != properties.property().end(); ++j)
            {
                // TODO add parameter to paramList here
            }
        }

        cameras.append(camera);
    }
}
