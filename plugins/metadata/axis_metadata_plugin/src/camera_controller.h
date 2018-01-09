#pragma once

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace nx {
namespace axis {

// ScopedGarbageCollector should be relocated to some other module. Ask Roma where to.
/**
* Scoped Garbage Collector.
* Declare
* Usage:
* 1. Declare garbage collector, e.g.
*     <code>ScopedGarbageCollector gc;</code>
* 2. Create objects with "create" member-function, e.g.
* <pre><code>
*     int* i = gc.create<int>;
*     std::string* s1 = gc.create<std::string>("Hello!");
*     auto s2 = gc.create<std::string>("Bye!");
*     MyClass* myClass = gc.create<MyClass>(100, 200, "Ok");
* </code></pre>
* 3. Objects will be deleted when gc leaves it scope.
* 4. Information: objects are deleted in reversed order: last created id destroyed first.
*/
class ScopedGarbageCollector final
{
    class BaseDeleter
    {
    public:
        virtual ~BaseDeleter() = default;
    };

    template<class T>
    class Deleter: public BaseDeleter
    {
        T* m_object;
    public:
        Deleter(T* object) noexcept : m_object(object) {}
        virtual ~Deleter() override { delete m_object; }
    };

    std::vector<BaseDeleter*> m_deleters;

public:
    ~ScopedGarbageCollector()
    {
        std::for_each(m_deleters.rbegin(), m_deleters.rend(), [](BaseDeleter* d) { delete d; });
    }

    template<class T, class ...Args>
    T* create(Args&& ... args)
    {
        // auto_ptr provides exception safity if code below throws exception.
        auto object = std::make_unique<T>(std::forward<Args>(args)...);

        // Separate push_back call provides exception safity if push_back fails.
        m_deleters.push_back(nullptr);
        m_deleters.back() = new Deleter<T>(object.get());

        return object.release();
    }
};

struct NameTypeParameter
{
    std::string name;
    std::string type;

    NameTypeParameter() = default;

    NameTypeParameter(const char* name, const char* type):
        name(name ? name : ""),
        type(type ? type : "")
    {
    }

    std::string toString() const { return name + " : " + type; }
};

struct NameValueParameter
{
    std::string name;
    std::string value;

    NameValueParameter() = default;

    NameValueParameter(const char* name, const char* value):
        name(name ? name : ""),
        value(value ? value : "")
    {
    }

    std::string toString() const { return name + " = " + value; }
};

struct SupportedEvent
{
    std::string topic; //< e.g. "tns1:Device"
    std::string name; //< e.g. "tnsaxis:Status/Temperature/Above"
    std::string description; //< e.g. "Above operating temperature"
    bool stateful = false;

    SupportedEvent() = default;

    SupportedEvent(const char* topic, const char* name, const char* description, bool stateful):
        topic(topic ? topic : ""),
        name(name ? name : ""),
        description(description ? description : ""),
        stateful(stateful)
    {
    }

    std::string fullName() const { return topic + '/' + name; }

    // Sometimes the result of "toString()" looks weird, it is not an error - the result shows the
    // real state of affairs.
    std::string toString() const; //< output format: <stateful:1 topic/name:64 description:any>
};

struct SupportedAction
{
    std::string token;
    std::string recipient;
    std::vector<NameTypeParameter> parameters;

    SupportedAction() = default;

    SupportedAction(
        const char* token,
        const char* recipient,
        std::vector<NameTypeParameter> parameters)
        :
        token(token ? token : ""),
        recipient(recipient ? recipient : ""),
        parameters(std::move(parameters))
    {
    }

    std::string toString() const;
};

struct SupportedRecipient
{
    std::string token;
    std::vector<NameTypeParameter> parameters;

    SupportedRecipient() = default;

    SupportedRecipient(const char* token): token(token) {}

    SupportedRecipient(const char* token, std::vector<NameTypeParameter> parameters):
        token(token ? token : ""),
        parameters(std::move(parameters))
    {
    }

    std::string toString() const;
};

struct ActiveAction
{
    int id;
    std::string name;
    std::string token;
    std::vector<NameValueParameter> parameters;

    explicit ActiveAction(int id=0): id(id) {}

    ActiveAction(
        int id,
        const char* name,
        const char* token,
        std::vector<NameValueParameter> parameters)
        :
        id(id),
        name(name ? name : ""),
        token(token ? token : ""),
        parameters(std::move(parameters))
    {
    }

    ActiveAction(const char* name, const char* token, std::vector<NameValueParameter> parameters):
        ActiveAction(0, name, token, std::move(parameters))
    {
    }
};

struct ActiveRule
{
    int id;
    std::string name;
    bool enabled;
    std::string eventName;
    // Axis rule also contains info about conditions, but we don't need it now.
    //vector<?> conditions;
    int actionId;

    explicit ActiveRule(int id = 0): id(id), actionId(0) {}

    ActiveRule(int id, const char* name, bool enabled, const char* eventName, int actionId):
        id(id),
        name(name ? name : ""),
        enabled(enabled),
        eventName(eventName ? eventName : ""),
        actionId(actionId)
    {
    }

    ActiveRule(const char* name, bool enabled, const char* eventName, int actionId):
        ActiveRule(0, name, enabled, eventName, actionId)
    {
    }
};

struct ActiveRecipient
{
    int id;
    std::string name;
    std::string token;
    std::vector<NameValueParameter> parameters;

    explicit ActiveRecipient(int id = 0): id(id) {}

    ActiveRecipient(
        int id,
        const char* name,
        const char* token,
        std::vector<NameValueParameter> parameters)
        :
        id(id),
        name(name ? name : ""),
        token(token ? token : ""),
        parameters(std::move(parameters))
    {
    }

    ActiveRecipient(
        const char* name,
        const char* token,
        std::vector<NameValueParameter> parameters)
        :
        ActiveRecipient(0, name, token, std::move(parameters))
    {
    }

};



class CameraController
{
public:
    CameraController() = default;
    CameraController(const char* ip);
    CameraController(const char* ip, const char* user, const char* password);
    ~CameraController() = default;

    void setIp(const char* ip);
    void setUserPassword(const char* user, const char* password);

    const char* ip() const noexcept { return m_ip.c_str(); }
    const char* endpoint() const noexcept { return m_endpoint.c_str(); }
    const char* user() const noexcept { return m_user.c_str(); }
    const char* password() const noexcept { return m_password.c_str(); }

    const std::vector<SupportedAction>& supportedActions() const noexcept
    {
        return m_supportedActions;
    }
    const std::vector<SupportedEvent>& suppotedEvents() const noexcept
    {
        return m_supportedEvents;
    }
    const std::vector<SupportedRecipient>& supportedRecipients() const noexcept
    {
        return m_supportedRecipients;
    }
    const std::vector<ActiveAction>& activeActions() const noexcept
    {
        return m_activeActions;
    }
    const std::vector<ActiveRule>& activeRules() const noexcept
    {
        return m_activeRules;
    }
    const std::vector<ActiveRecipient>& activeRecipients() const noexcept
    {
        return m_activeRecipients;
    }

    bool readSupportedActions();
    bool readSupportedEvents();
    bool readSupportedRecipients();

    bool readActiveActions();
    bool readActiveRules();
    bool readActiveRecipients();

    void filterSupportedEvents(const std::vector<std::string>& neededTopics);
    void filterSupportedEvents(std::initializer_list<const char*> neededTopics);

    // The methods which names begin with "add" or "remove" do not modify corresponding inner
    // vectors. To update a vector call appropriate read method.

    /** @return Id of the added action. */
    int addActiveAction(const ActiveAction& action);

    /**
     *NOTE: This high-level function gets out of low-level style of the class, but it's often
     * needed, so it is left here.
     */
    /** @return Id of the added action. */
    int addActiveHttpNotificationAction(
        const char* name,
        const char* message,
        const char* recipientEndpoint,
        const char* recipientLogin = nullptr,
        const char* recipientPassword = nullptr);

    /**
     * Warning: an action can not be removed if it is used in a rule, so if the action is "busy":
     * first remove the corresponding rule, then - the action.
     */
    bool removeActiveAction(int actionId);

    /** @return Id of the added recipient. */
    int addActiveRecipient(const ActiveRecipient& recipient);
    bool removeActiveRecipient(int recipientId);

    /** @return Id of the added rule. */
    int addActiveRule(const ActiveRule& rule);
    bool removeActiveRule(int ruleId);

    // Axiliary functions. Helpful to clean camera memory after work.
    // Waring: they do not empty corresponding vectors.
    int removeAllActiveActions(const char* namePrefix = nullptr);
    int removeAllActiveRecipients(const char* namePrefix = nullptr);
    int removeAllActiveRules(const char* namePrefix = nullptr);

private:
    void setEndpoint();

private:
    std::string m_ip;
    std::string m_endpoint;
    std::string m_user;
    std::string m_password;

    std::vector<SupportedAction> m_supportedActions;
    std::vector<SupportedEvent> m_supportedEvents;
    std::vector<SupportedRecipient> m_supportedRecipients;

    std::vector<ActiveAction> m_activeActions;
    std::vector<ActiveRule> m_activeRules;
    std::vector<ActiveRecipient> m_activeRecipients;
};

} // namespace axis
} // namespace nx
