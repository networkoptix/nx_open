#include "camera_controller.h"

#include "gsoap/generated_with_soapcpp2/soapEventBindingProxy.h"
#include "gsoap/generated_with_soapcpp2/soapActionBindingProxy.h"
#include "gsoap/generated_with_soapcpp2/EventBinding.nsmap"

#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include <tuple>

#include <nx/utils/thread/mutex.h>

#include "httpda.h" //< gsoap header for digest authentication

namespace nx {
namespace axis {

namespace {

template<class T>
struct ServiceGuard
{
    ServiceGuard(T& service) : m_service(service) {}
    ~ServiceGuard() { m_service.destroy(); }

private:
    T& m_service;
};
using EventServiceGuard = ServiceGuard<EventBindingProxy>;
using ActionServiceGuard = ServiceGuard<ActionBindingProxy>;

class SoapAuthentificator
{
public:
    SoapAuthentificator(struct soap* soap, const char* user, const char* password):
        m_soap(soap)
    {
        http_da_save(soap, &info, soap->authrealm, user, password);
    }

    ~SoapAuthentificator()
    {
        http_da_release(m_soap, &info);
    }

private:
    soap* m_soap;
    http_da_info info;
};

template<class Service, class Request, class Response>
bool makeSoapRequest(
    Service& service,
    int (Service::*method)(Request*, Response&),
    Request& request,
    Response& response,
    const char* user,
    const char* password)
{
    static const int kAuthenticationError = 401;

    static QnMutex soapMutex;
    QnMutexLocker locker(&soapMutex);

    // Plugin deregistering actions and memory deallocation run in ServiceGuard destructor.
    soap_register_plugin(service.soap, http_da);
    if ((service.*method)(&request, response) == kAuthenticationError)
    {
        // One more try with advanced authentication.
        SoapAuthentificator authentificator(service.soap, user, password);
        (service.*method)(&request, response);
    }
    return (service.soap->error == SOAP_OK);
}

/**
* RAII helper class that sets a value to a variable in constructor and restores variable's
* initial value in destructor. Usage example:
* <pre><code>
*     int a=1;
*     {
*         ValueRestorer<int> restorer<int>(a,2);
*         assert(a == 2);
*     }
*     assert(a == 1);
* </code></pre>
*/
template<class T>
class ValueRestorer
{
public:
    ValueRestorer(T& variable, T newValue) : m_variable(variable)
    {
        variable = newValue;
        m_oldValue = m_variable;
    }

    ~ValueRestorer()
    {
        m_variable = m_oldValue;
    }

private:
    T& m_variable;
    T m_oldValue;
};

/** Concatenates vector elements into a string, interleaving them with a separator. */
std::string joinTags(const std::vector<const char*>& st, char separator = '/')
{
    std::string result;
    for (const auto str: st)
    {
        result += str;
        result.push_back(separator);
    }
    if (!result.empty())
        result.pop_back();
    return result;
}

/**
 * Recursively finds all events in subtree "element" (if any) and appends them to "events" vector.
 * A "topic" member of every found event gets "topicName" value.
 */
void replanishEventsFromElement(const soap_dom_element* element, const char* topicName,
    std::vector<const char*>& tagStack, std::vector<nx::axis::SupportedEventType>& events)
{
    static const char kStopTag[] = "aev:MessageInstance";
    static const char kNiceName[] = "aev:NiceName";
    static const char kStatefulness[] = "aev:isProperty";
    static const char kStatefulnessTrueValue[] = "true";

    const bool weNeedToGoDeeper = element->elts && strcmp(element->name, kStopTag) != 0;
    if (!weNeedToGoDeeper)
    {
        soap_dom_attribute_iterator it;
        if (element->prnt)
            it = element->prnt->att_find(kNiceName);
        const char* const niceName = it.iter ? it->text : "";

        const soap_dom_attribute* const attr = element->att_get(element->nstr, kStatefulness);
        const bool isStateful = attr && attr->text && !strcmp(attr->text, kStatefulnessTrueValue);

        const std::string fullName = joinTags(tagStack);
        events.emplace_back(topicName, fullName.c_str(), niceName, isStateful);
        return;
    }

    tagStack.push_back(element->name);
    element = element->elts;
    while (element)
    {
        replanishEventsFromElement(element, topicName, tagStack, events);
        element = element->next;
    }
    tagStack.pop_back();
}

/** Find events in topic "topic" and add them to vector "events". */
void replanishEventsFromTopic(const soap_dom_element* topic, const char* topicName,
    std::vector<nx::axis::SupportedEventType>& events)
{
    std::vector<const char*> auxiliaryStack;
    const soap_dom_element* child = topic->elts;
    while (child)
    {
        auxiliaryStack.clear();
        replanishEventsFromElement(child, topic->name, auxiliaryStack, events);
        child = child->next;
    }
}

bool startsWith(const std::string& name, const std::string& prefix)
{
    if (prefix.empty())
        return true;
    const auto it = std::search(name.cbegin(), name.cend(), prefix.cbegin(), prefix.cend());
    return it == name.cbegin();
}

/** Calculate how many namespaces are there in the array. */
int countNamespaces(const Namespace* start)
{
    int count = 0;
    while (start[count].id)
        ++count;
    return count;
}

std::vector<Namespace> buildExtentedNamespaceArray(const Namespace* baseNamespaceArray)
{
    // There are baseNamespaceCount namespaces and one null namespace indicating the end of array.
    int baseNamespaceCount = countNamespaces(baseNamespaceArray);

    static const int kAdditionalNamespaceCount = 3;
    static const Namespace kAdditionalNamespaces[kAdditionalNamespaceCount + 1] = {
        {"tns1", "http://www.onvif.org/ver10/topics", nullptr, nullptr},
        {"wsnt", "http://docs.oasis-open.org/wsn/b-2", nullptr, nullptr},
        {"tnsaxis", "http://www.axis.com/2009/event/topics", nullptr, nullptr},
        {nullptr, nullptr, nullptr, nullptr}};

    std::vector<Namespace> newNamespaceArray(
        baseNamespaceArray, baseNamespaceArray + baseNamespaceCount);
    std::copy_n(kAdditionalNamespaces, kAdditionalNamespaceCount + 1,
        std::back_inserter(newNamespaceArray));

    return newNamespaceArray;
}

} // namespace

std::string SupportedEventType::toString() const
{
    std::stringstream output;
    output << (stateful ? 1 : 0) << ' ' << std::setw(64) << std::left << fullName() << description;
    return output.str();
}

std::string SupportedAction::toString() const
{
    std::stringstream ss;
    ss << std::setw(32) << std::left << token << ' ' << recipient;
    for (const auto& parameter: parameters)
    {
        ss << "\n\t" << parameter.toString();
    }
    return ss.str();
}

std::string SupportedRecipient::toString() const
{
    std::stringstream ss;
    ss << std::left << token;
    for (const auto& parameter: parameters)
    {
        ss << "\n\t" << parameter.toString();
    }
    return ss.str();
}

CameraController::CameraController(const char* ip):
    m_ip(ip ? ip : "")
{
    setEndpoint();
}

CameraController::CameraController(const char* ip, const char* user, const char* password):
    m_ip(ip ? ip : ""), m_user(user ? user : ""), m_password(password ? password : "")
{
    setEndpoint();
}

void CameraController::setIp(const char* ip)
{
    m_ip = ip ? ip : "";
    setEndpoint();
}

void CameraController::setUserPassword(const char* user, const char* password)
{
    m_user = user ? user : "";
    m_password = password ? password : "";
}

void CameraController::setEndpoint()
{
    static const std::string kProtocol("http://");
    static const std::string kPath("/vapix/services");
    m_endpoint = kProtocol + m_ip + kPath;
}

bool CameraController::readSupportedActions()
{
    ActionBindingProxy service(endpoint());
    ActionServiceGuard serviceGuard(service);
    _ns5__GetActionTemplates request;
    _ns5__GetActionTemplatesResponse response;
    if (!makeSoapRequest(service, &ActionBindingProxy::GetActionTemplates, request, response,
        m_user.c_str(), m_password.c_str()))
        return false;

    std::vector<SupportedAction> actions;
    for (const ns5__ActionTemplate* const item: response.ActionTemplates->ActionTemplate)
    {
        std::vector<NameTypeParameter> parameters;
        if (item->Parameters)
        {
            for (const ns5__ActionTemplateParameter* const parameter: item->Parameters->Parameter)
            {
                parameters.emplace_back(parameter->Name.c_str(), parameter->Type.c_str());
            }
        }
        actions.emplace_back(
            item->TemplateToken.c_str(),
            item->RecipientTemplate ? item->RecipientTemplate->c_str() : "",
            parameters);
    }
    m_supportedActions = std::move(actions);
    return true;
}

void CameraController::filterSupportedEvents(const std::vector<std::string>& neededTopics)
{
    if (neededTopics.empty())
        return;
    std::vector<SupportedEventType> events;
    std::copy_if(m_supportedEvents.cbegin(), m_supportedEvents.cend(), std::back_inserter(events),
        [&neededTopics](const auto& event)
    {
        return find(neededTopics.cbegin(), neededTopics.cend(), event.topic) !=
            neededTopics.cend();
    });
    m_supportedEvents = std::move(events);
}

void CameraController::removeForbiddenEvents(const std::vector<std::string>& forbiddenDescriptions)
{
    if (forbiddenDescriptions.empty())
        return;
    std::vector<SupportedEventType> events;
    std::copy_if(m_supportedEvents.cbegin(), m_supportedEvents.cend(), std::back_inserter(events),
        [&forbiddenDescriptions](const auto& event)
    {
        return find(forbiddenDescriptions.cbegin(), forbiddenDescriptions.cend(),
            event.description) == forbiddenDescriptions.cend();
    });
    m_supportedEvents = std::move(events);
}

void CameraController::filterSupportedEvents(std::initializer_list<const char*> neededTopics)
{
    std::vector<std::string> filter;
    std::transform(neededTopics.begin(), neededTopics.end(), std::back_inserter(filter),
        [](const char* in) { return in ? in : ""; });
    filterSupportedEvents(filter);
}

bool CameraController::readSupportedEvents()
{
    EventBindingProxy service(endpoint());
    EventServiceGuard serviceGuard(service);
    _ns1__GetEventInstances request;
    _ns1__GetEventInstancesResponse response;
    if (!makeSoapRequest(service, &EventBindingProxy::GetEventInstances, request, response,
        m_user.c_str(), m_password.c_str()))
        return false;

    std::vector<SupportedEventType> events;
    for (const soap_dom_element& topic: response.ns2__TopicSet->__any)
    {
        replanishEventsFromTopic(&topic, topic.name, events);
    }
    m_supportedEvents = std::move(events);
    return true;
}

bool CameraController::readSupportedRecipients()
{
    ActionBindingProxy service(endpoint());
    ActionServiceGuard serviceGuard(service);
    _ns5__GetRecipientTemplates request;
    _ns5__GetRecipientTemplatesResponse response;
    if (!makeSoapRequest(service, &ActionBindingProxy::GetRecipientTemplates, request, response,
        m_user.c_str(), m_password.c_str()))
        return false;

    std::vector<SupportedRecipient> recipients;
    for (const ns5__RecipientTemplate* const item: response.RecipientTemplates->RecipientTemplate)
    {
        std::vector<NameTypeParameter> parameters;
        if (item->Parameters)
        {
            for (const ns5__ActionTemplateParameter* const parameter : item->Parameters->Parameter)
            {
                parameters.emplace_back(parameter->Name.c_str(), parameter->Type.c_str());
            }
        }
        recipients.emplace_back(item->TemplateToken.c_str(), std::move(parameters));
    }
    m_supportedRecipients = std::move(recipients);
    return true;
}

bool CameraController::readActiveActions()
{
    ActionBindingProxy service(endpoint());
    ActionServiceGuard serviceGuard(service);
    _ns5__GetActionConfigurations request;
    _ns5__GetActionConfigurationsResponse response;
    if (!makeSoapRequest(service, &ActionBindingProxy::GetActionConfigurations, request, response,
        m_user.c_str(), m_password.c_str()))
    {
        return false;
    }

    std::vector<ActiveAction> actions;
    for (const ns5__ActionConfiguration* const configuration:
        response.ActionConfigurations->ActionConfiguration)
    {
        std::vector<NameValueParameter> parameters;
        if (configuration->Parameters)
        {
            for (const ns5__ActionParameter* const parameter: configuration->Parameters->Parameter)
            {
                parameters.emplace_back(parameter->Name.c_str(), parameter->Value.c_str());
            }
        }
        actions.emplace_back(
            atoi(configuration->ConfigurationID.c_str()),
            configuration->Name ? configuration->Name->c_str() : "",
            configuration->TemplateToken.c_str(),
            parameters);
    }
    m_activeActions = std::move(actions);
    return true;
}

bool CameraController::readActiveRules()
{
    ActionBindingProxy service(endpoint());
    ActionServiceGuard serviceGuard(service);
    _ns5__GetActionRules request;
    _ns5__GetActionRulesResponse response;
    if (!makeSoapRequest(service, &ActionBindingProxy::GetActionRules, request, response,
        m_user.c_str(), m_password.c_str()))
    {
        return false;
    }

    std::vector<ActiveRule> rules;
    for (const ns5__ActionRule* rule: response.ActionRules->ActionRule)
    {
        // We don't need information about conditions in current version, so we just ignore it.
        rules.emplace_back(
            atoi(rule->RuleID.c_str()),
            rule->Name ? rule->Name->c_str() : "",
            rule->Enabled,
            (rule->StartEvent && !(rule->StartEvent->__any.empty()))
                ? rule->StartEvent->__any[0].text
                : "",
            atoi(rule->PrimaryAction.c_str()));
    }
    m_activeRules = std::move(rules);
    return true;
}

bool CameraController::readActiveRecipients()
{
    ActionBindingProxy service(endpoint());
    ActionServiceGuard serviceGuard(service);
    _ns5__GetRecipientConfigurations request;
    _ns5__GetRecipientConfigurationsResponse response;
    if (!makeSoapRequest(service, &ActionBindingProxy::GetRecipientConfigurations, request,
        response, m_user.c_str(), m_password.c_str()))
    {
        return false;
    }

    std::vector<ActiveRecipient> recipients;
    for (const ns5__RecipientConfiguration* const configuration:
        response.RecipientConfigurations->RecipientConfiguration)
    {
        std::vector<NameValueParameter> parameters;
        if (configuration->Parameters)
        {
            for (const ns5__ActionParameter* const parameter: configuration->Parameters->Parameter)
            {
                parameters.emplace_back(parameter->Name.c_str(), parameter->Value.c_str());
            }
        }
        recipients.emplace_back(
            atoi(configuration->ConfigurationID.c_str()),
            configuration->Name ? configuration->Name->c_str() : "",
            configuration->TemplateToken.c_str(),
            parameters);
    }
    m_activeRecipients = std::move(recipients);
    return true;
}

int CameraController::addActiveAction(const ActiveAction& action)
{
    ActionBindingProxy service(endpoint());
    ActionServiceGuard serviceGuard(service);
    _ns5__AddActionConfiguration addCommand;
    _ns5__AddActionConfigurationResponse response;

    nx::utils::ScopedGarbageCollector gc;
    addCommand.NewActionConfiguration = gc.create<ns5__NewActionConfiguration>();
    addCommand.NewActionConfiguration->Name = gc.create<std::string>(action.name);

    // Token should have a value like "com.axis.action.fixed.notification.http".
    addCommand.NewActionConfiguration->TemplateToken = action.token;

    addCommand.NewActionConfiguration->Parameters = gc.create<ns5__ActionParameters>();
    std::vector<ns5__ActionParameter*>& params =
        addCommand.NewActionConfiguration->Parameters->Parameter;
    params.resize(action.parameters.size());
    for (int i = 0; i < params.size(); ++i)
    {
        params[i] = gc.create<ns5__ActionParameter>();
        params[i]->Name = action.parameters[i].name;
        params[i]->Value = action.parameters[i].value;
    }

    if (!makeSoapRequest(service, &ActionBindingProxy::AddActionConfiguration, addCommand,
        response, m_user.c_str(), m_password.c_str()))
        return 0;

    // If "response.ConfigurationID" is not an integer, atoi() returns 0. It suits us
    // because 0 means that insertion has failed.
    return atoi(response.ConfigurationID.c_str());
}

int CameraController::addActiveHttpNotificationAction(const char* name, const char* message,
    const char* recipientEndpoint, const char* recipientLogin, const char* recipientPassword)
{
    static const char* const kNotificationToken = "com.axis.action.fixed.notification.http";
    const std::vector<NameValueParameter> parameters = {
        // action parameters
        {"message", message ? message : ""},
        {"parameters", ""},
        // recipient parameters
        {"upload_url", recipientEndpoint ? recipientEndpoint : ""},
        {"login", recipientLogin ? recipientLogin : ""},
        {"password", recipientPassword ? recipientPassword : ""},
        {"proxy_host", ""},
        {"proxy_port", ""},
        {"proxy_login", ""},
        {"proxy_password", ""},
        //{"validate_server_cert", ""}, //< In practice, should not be added.
        {"qos", "0"},
    };
    const ActiveAction action(name, kNotificationToken, parameters);
    return addActiveAction(action);
}

bool CameraController::removeActiveAction(int actionId)
{
    ActionBindingProxy service(endpoint());
    ActionServiceGuard serviceGuard(service);
    _ns5__RemoveActionConfiguration removeCommand;
    _ns5__RemoveActionConfigurationResponse response;
    removeCommand.ConfigurationID = std::to_string(actionId);
    return makeSoapRequest(service, &ActionBindingProxy::RemoveActionConfiguration, removeCommand,
        response, m_user.c_str(), m_password.c_str());
}

int CameraController::addActiveRecipient(const ActiveRecipient& recipient)
{
    ActionBindingProxy service(endpoint());
    ActionServiceGuard serviceGuard(service);
    _ns5__AddRecipientConfiguration addCommand;
    _ns5__AddRecipientConfigurationResponse response;

    nx::utils::ScopedGarbageCollector gc;
    addCommand.NewRecipientConfiguration = gc.create<ns5__NewRecipientConfiguration>();
    addCommand.NewRecipientConfiguration->Name = gc.create<std::string>(recipient.name);
    addCommand.NewRecipientConfiguration->TemplateToken = recipient.token;
    addCommand.NewRecipientConfiguration->Parameters = gc.create<ns5__ActionParameters>();

    std::vector<ns5__ActionParameter*>& params =
        addCommand.NewRecipientConfiguration->Parameters->Parameter;
    params.resize(recipient.parameters.size());
    for (int i = 0; i < params.size(); ++i)
    {
        params[i] = gc.create<ns5__ActionParameter>();
        params[i]->Name = recipient.parameters[i].name;
        params[i]->Value = recipient.parameters[i].value;
    }

    if (!makeSoapRequest(service, &ActionBindingProxy::AddRecipientConfiguration, addCommand,
        response, m_user.c_str(), m_password.c_str()))
        return 0;

    // If "response.ConfigurationID" is not an integer, atoi() returns 0. It suits us
    // because 0 means that insertion failed.
    return atoi(response.ConfigurationID.c_str());
}

bool CameraController::removeActiveRecipient(int recipientId)
{
    ActionBindingProxy service(endpoint());
    ActionServiceGuard serviceGuard(service);
    _ns5__RemoveRecipientConfiguration removeCommand;
    _ns5__RemoveRecipientConfigurationResponse response;
    removeCommand.ConfigurationID = std::to_string(recipientId);
    return makeSoapRequest(service, &ActionBindingProxy::RemoveRecipientConfiguration,
        removeCommand, response, m_user.c_str(), m_password.c_str());
}

int CameraController::addActiveRule(const ActiveRule& rule)
{
    ActionBindingProxy service(endpoint());
    ActionServiceGuard serviceGuard(service);
    _ns5__AddActionRule addCommand;
    _ns5__AddActionRuleResponse response;

    /*
    The example of what is needed:
    <StartEvent>
        <wsnt:TopicExpression Dialect="http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet">
            tns1:VideoSource/tnsaxis:MotionDetection
        </wsnt:TopicExpression>
    </StartEvent>
    */

    nx::utils::ScopedGarbageCollector gc;
    addCommand.NewActionRule = gc.create<ns5__NewActionRule>();
    addCommand.NewActionRule->Name = gc.create<std::string>(rule.name);
    addCommand.NewActionRule->Enabled = true;
    addCommand.NewActionRule->PrimaryAction = std::to_string(rule.actionId);
    addCommand.NewActionRule->StartEvent = gc.create<ns3__FilterType>();

    const char kRuleTagName[] = "wsnt:TopicExpression";
    const char* const ruleTagValue = rule.eventName.c_str();
    const char kRuleAttributeName[] = "Dialect";
    const char kRuleAttributeValue[] =
        "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet";

    soap_dom_element topicExpression(service.soap, NULL, kRuleTagName, ruleTagValue);
    topicExpression.atts = gc.create<soap_dom_attribute>(
        service.soap, nullptr, kRuleAttributeName, kRuleAttributeValue);
    addCommand.NewActionRule->StartEvent->__any.push_back(topicExpression);

    // We need to temporarily enlarge namespace array to execute the soap request correctly.
    const std::vector<Namespace> extendedNamespaceArray =
        buildExtentedNamespaceArray(service.soap->namespaces);
    const ValueRestorer<const Namespace*> valueRestorer(
        service.soap->namespaces, &extendedNamespaceArray.front());

    if (!makeSoapRequest(service, &ActionBindingProxy::AddActionRule, addCommand,
        response, m_user.c_str(), m_password.c_str()))
        return 0;

    // If "response.RuleID" is not an integer, atoi() returns 0. It suits us because 0 means that
    // insertion failed.
    return atoi(response.RuleID.c_str());
}

bool CameraController::removeActiveRule(int ruleId)
{
    ActionBindingProxy service(endpoint());
    ActionServiceGuard serviceGuard(service);
    _ns5__RemoveActionRule removeCommand;
    _ns5__RemoveActionRuleResponse response;
    removeCommand.RuleID = std::to_string(ruleId);
    return makeSoapRequest(service, &ActionBindingProxy::RemoveActionRule, removeCommand, response,
        m_user.c_str(), m_password.c_str());
}

int CameraController::removeAllActiveActions(const char* namePrefix)
{
    if (!readActiveActions())
        return 0;

    int counter = 0;
    const std::string prefix(namePrefix ? namePrefix : "");
    for (const auto& action: m_activeActions)
    {
        if (startsWith(action.name, prefix) && removeActiveAction(action.id))
            counter++;
    }
    return counter;
}

int CameraController::removeAllActiveRecipients(const char* namePrefix)
{
    if (!readActiveRecipients())
        return 0;

    int counter = 0;
    const std::string prefix(namePrefix ? namePrefix : "");
    for (const auto& recipient: m_activeRecipients)
    {
        if (startsWith(recipient.name, prefix) && removeActiveRecipient(recipient.id))
            ++counter;
    }
    return counter;
}

int CameraController::removeAllActiveRules(const char* namePrefix)
{
    if (!readActiveRules())
        return 0;

    int counter = 0;
    const std::string prefix(namePrefix ? namePrefix : "");
    for (const auto& rule: m_activeRules)
    {
        if (startsWith(rule.name, prefix) && removeActiveRule(rule.id))
            counter++;
    }
    return counter;
}

} // namespace axis
} // namespace nx
