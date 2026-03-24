#pragma once

#include <exception>
#include <list>
#include <map>
#include <memory>
#include <sstream>

#include <nx/utils/log/log.h>

namespace nx::analytics::app_utils {

struct NX_UTILS_API OptionCont
{
    typedef std::list<std::string> StringList;

    OptionCont operator||(const OptionCont& right) noexcept;

    StringList equalOptions;
    StringList shortOptions;
};

class CheckOption
{
public:
    CheckOption() noexcept;

    bool requireValue() const noexcept;

    bool enabled() const noexcept;

    void set(const char* optName, const char* val) noexcept;

protected:
    bool m_enabled;
};

template <typename Object>
class Option
{
public:
    Option() noexcept;

    explicit Option(const Object& val) noexcept;

    const Object& operator*() const noexcept;

    const Object* operator->() const noexcept;

    bool requireValue() const noexcept;

    bool installed() const noexcept;

    void set(const char* optName, const char* val);

    void setValue(const Object& val) noexcept;

protected:
    Object m_val;
    bool m_installed;
};

/**
 * The same as Option<> but each call to the set() method saves options to the container that
 * supports push_back(). That is the Object type must support the push_back() operation.
 */
template <typename Object>
class OptionsSet: public Option<Object>
{
public:
    typedef typename Object::value_type ValueType;

    /** Constructs an option container object in a not-installed state. */
    OptionsSet() noexcept;

    /** Constructs an option container object in a not-installed state, and assigns a value. */
    explicit OptionsSet(const Object& val) noexcept;

    /** Parses the source and stores the value read to the OptionsSet object. */
    void set(const char* optName, const char* val);
};

class NX_UTILS_API StringOption: public Option<std::string>
{
public:
    StringOption() noexcept;

    StringOption(const std::string& val) noexcept;

    void set(const char* optName, const char* val);
};

/**
 * Application command line option parsing helper.
 */
class NX_UTILS_API Args
{
public:
    typedef std::list<std::string> CommandList;

    explicit Args(int command_count = 0);

    template <typename Option>
    void add(const OptionCont& cont, Option& opt, const char* comment = 0,
        const char* arg_name = 0);

    void parse(int argc, const char* const argv[]);

    const CommandList& commands() const noexcept;

    void usage(std::ostream& output) const;

protected:
    struct ParseState
    {
    public:
        ParseState(int argcval, const char* const* argvval, const char* argPosval)
            noexcept;

        bool nextWord() noexcept;

        bool end() noexcept;

        const char* currentPos() noexcept;

        void currentPos(const char* pos) noexcept;

    protected:
        int m_argc;
        const char* const* m_argv;
        const char* m_argPos;
    };

    class OptionSetter
    {
    public:
        virtual ~OptionSetter() noexcept;

        virtual void set(const char* optName, const char* val) = 0;

        virtual bool requireValue() const noexcept = 0;
    };

    typedef std::shared_ptr<OptionSetter> OptionSetter_var;

    typedef std::map<std::string, OptionSetter_var> OptionSetterMap;

    bool parseEqOp(ParseState& parse_state);

    bool parseShortOptSeq(ParseState& parse_state);

    bool parseShortOpt(ParseState& parse_state);

    void parseShortOpValue(OptionSetterMap::iterator it, const char* optName,
        ParseState& parse_state);

private:
    template <typename Option>
    class OptionSetterImpl: public OptionSetter
    {
    public:
        OptionSetterImpl(Option& opt) noexcept;

        virtual ~OptionSetterImpl() noexcept;

        virtual void set(const char* optName, const char* val);

        virtual bool requireValue() const noexcept;

    private:
        Option& m_opt;
    };

    static void appendFlag(const std::string& flag, bool short_opt, std::string& flags,
        std::string& usage);

    typedef std::map<std::string, std::string> Usage;

    int m_commandCount;
    CommandList m_commands;
    OptionSetterMap m_equalOptions;
    OptionSetterMap m_shortOptions;
    Usage m_usage;
};

inline OptionCont equalName(const char* name)
{
    OptionCont result;
    result.equalOptions.push_back(name);
    return result;
}

inline OptionCont shortName(const char* name)
{
    OptionCont result;
    result.shortOptions.push_back(name);
    return result;
}

//-------------------------------------------------------------------------------------------------
// CheckOption class inline methods implementation.

inline CheckOption::CheckOption() noexcept:
    m_enabled(false)
{
}

inline bool CheckOption::requireValue() const noexcept
{
    return false;
}

inline bool CheckOption::enabled() const noexcept
{
    return m_enabled;
}

inline void CheckOption::set(const char* /*optName*/, const char* /*val*/) noexcept
{
    m_enabled = true;
}

//-------------------------------------------------------------------------------------------------
// Option class inline methods implementation.

template <typename Object>
Option<Object>::Option() noexcept:
    m_installed(false)
{
}

template <typename Object>
Option<Object>::Option(const Object& val) noexcept:
    m_val(val), m_installed(false)
{
}

template <typename Object>
const Object& Option<Object>::operator*() const noexcept
{
    return m_val;
}

template <typename Object>
const Object* Option<Object>::operator->() const noexcept
{
    return &m_val;
}

template <typename Object>
bool Option<Object>::requireValue() const noexcept
{
    return true;
}

template <typename Object>
bool Option<Object>::installed() const noexcept
{
    return m_installed;
}

template <typename Object>
void Option<Object>::set(const char* /*optName*/, const char* val)
{
    static const char* const kFunctionName = "AppUtils::String<>::set()";

    if (installed())
    {
        throw std::runtime_error(nx::format("%1: Second time defined value '%2'").arg(
            kFunctionName).arg(val).toStdString());
    }

    std::istringstream inputStream{std::string(val)};
    inputStream >> m_val;
    if (inputStream.bad() || (inputStream.peek(), !inputStream.eof()))
    {
        throw std::runtime_error(nx::format("%1: Bad value '%2'").arg(kFunctionName).arg(
            val).toStdString());
    }

    m_installed = true;
}

template <typename Object>
void Option<Object>::setValue(const Object& val) noexcept
{
    m_val = val;
    m_installed = true;
}

//-------------------------------------------------------------------------------------------------
// OptionsSet class inline methods implementation.

template <typename Object>
OptionsSet<Object>::OptionsSet() noexcept
{
}

template <typename Object>
OptionsSet<Object>::OptionsSet(const Object& val) noexcept:
    Option<Object>(val)
{
}

template <typename Object>
void OptionsSet<Object>::set(const char* /*optName*/, const char* val)
{
    static const char* const kFunctionName = "AppUtils::OptionsSet<>::set()";

    std::istringstream inputStream(val);
    ValueType value;
    inputStream >> value;
    if (inputStream.bad() || !inputStream.eof())
    {
        throw std::runtime_error(nx::format("%1: Bad value '%2'").arg(
            kFunctionName).arg(val).toStdString());
    }

    Option<Object>::m_val.push_back(value);
    Option<Object>::m_installed = true;
}

//-------------------------------------------------------------------------------------------------
// StringOption class inline methods implementation.

inline StringOption::StringOption() noexcept
{
}

inline StringOption::StringOption(const std::string& val) noexcept:
    Option<std::string>(val)
{
}

inline void StringOption::set(const char* /*optName*/, const char* val)
{
    static const char* const kFunctionName = "AppUtils::StringOption::set()";

    if (installed())
    {
        throw std::runtime_error(nx::format("%1: Second time defined value '%2'").arg(
            kFunctionName).arg(val).toStdString());
    }

    m_val = val;
    m_installed = true;
}

//-------------------------------------------------------------------------------------------------
// Args::OptionSetter class inline methods implementation.

inline Args::OptionSetter::~OptionSetter() noexcept
{
}

//-------------------------------------------------------------------------------------------------
// Args::OptionSetterImpl class inline methods implementation.

template <typename Option>
Args::OptionSetterImpl<Option>::OptionSetterImpl(Option& opt) noexcept:
    m_opt(opt)
{
}

template <typename Option>
Args::OptionSetterImpl<Option>::~OptionSetterImpl() noexcept
{
}

template <typename Option>
void Args::OptionSetterImpl<Option>::set(const char* optName, const char* val)
{
    m_opt.set(optName, val);
}

template <typename Option>
bool Args::OptionSetterImpl<Option>::requireValue() const noexcept
{
    return m_opt.requireValue();
}

//-------------------------------------------------------------------------------------------------
// Args class inline methods implementation.

inline Args::Args(int command_count):
    m_commandCount(command_count)
{
}

inline const Args::CommandList& Args::commands() const noexcept
{
    return m_commands;
}

inline void Args::appendFlag(const std::string& flag, bool short_opt, std::string& flags,
    std::string& usage)
{
    flags.append(flag);
    if (!usage.empty())
        usage.push_back(',');
    usage.append(short_opt ? " -" : " --");
    usage.append(flag);
}

template <typename Option>
void Args::add(const OptionCont& cont, Option& opt, const char* comment, const char* arg_name)
{
    std::string flags, usage;

    for (OptionCont::StringList::const_iterator it = cont.shortOptions.begin();
        it != cont.shortOptions.end(); ++it)
    {
        m_shortOptions.insert(OptionSetterMap::value_type(
            *it,
            OptionSetter_var(new OptionSetterImpl<Option>(opt))));
        appendFlag(*it, true, flags, usage);
    }

    for (OptionCont::StringList::const_iterator it = cont.equalOptions.begin();
         it != cont.equalOptions.end(); ++it)
    {
        m_equalOptions.insert(OptionSetterMap::value_type(*it,
            OptionSetter_var(new OptionSetterImpl<Option>(opt))));
        appendFlag(*it, false, flags, usage);
    }

    if (opt.requireValue())
    {
        if (arg_name)
        {
            usage.append("=");
            usage.append(arg_name);
        }
        else
        {
            usage.append("=arg");
        }
    }

    if (comment)
    {
        usage.append(" - ");
        usage.append(comment);
    }

    m_usage[flags].swap(usage);
}

} // namespace nx::analytics
