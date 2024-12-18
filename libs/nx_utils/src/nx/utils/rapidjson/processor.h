// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QString>

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/writer.h>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/buffer.h>
#include <nx/utils/std_string_utils.h>

namespace nx::utils::rapidjson {

/** Own implementation of std::convertible_to for old compiler versions. */
template<class From, class To>
concept convertible_to =
    std::is_convertible_v<From, To> && requires { static_cast<To>(std::declval<From>()); };

using ArrayIterator = ::rapidjson::Value::ValueIterator;
using ObjectIterator = ::rapidjson::Value::MemberIterator;

template<typename T>
concept IsArrayIterator = std::is_same_v<T, ArrayIterator>;

template<typename T>
concept IsObjectIterator = std::is_same_v<T, ObjectIterator>;

template<typename T>
concept RapidjsonIteratorType = IsArrayIterator<T> || IsObjectIterator<T>;

template<typename T>
concept ArrayPredicateType = requires(T t, ArrayIterator& valueIt)
{
    { t(valueIt) } -> convertible_to<bool>;
};

template<typename T>
concept ObjectPredicateType = requires(T t, ObjectIterator& memberIt)
{
    { t(memberIt) } -> convertible_to<bool>;
};

template<typename T>
concept PredicateType = ArrayPredicateType<T> || ObjectPredicateType<T>;

enum class NX_UTILS_API ElementWithExistingName
{
    add,
    ignore,
    replace
};

namespace details {

template<typename T>
struct TypeHelper
{
    using OutputType = T;

    static std::optional<OutputType> get(::rapidjson::Value* jsonNode)
    {
        if (!jsonNode)
            return std::nullopt;

        OutputType result;
        nx::reflect::json::DeserializationContext ctx{
            *jsonNode, (int) nx::reflect::json::DeserializationFlag::none};
        if (!nx::reflect::json::deserialize<OutputType>(ctx, &result))
            return std::nullopt;
        return result;
    }
};

template<>
struct TypeHelper<int>
{
    using OutputType = int;

    static std::optional<OutputType> get(::rapidjson::Value* jsonNode)
    {
        if (jsonNode && jsonNode->IsInt())
            return jsonNode->GetInt();
        return std::nullopt;
    }
};

template<>
struct TypeHelper<bool>
{
    using OutputType = bool;

    static std::optional<OutputType> get(::rapidjson::Value* jsonNode)
    {
        if (jsonNode && jsonNode->IsBool())
            return jsonNode->GetBool();
        return std::nullopt;
    }
};

template<>
struct TypeHelper<double>
{
    using OutputType = double;

    static std::optional<OutputType> get(::rapidjson::Value* jsonNode)
    {
        if (jsonNode && jsonNode->IsDouble())
            return jsonNode->GetDouble();
        return std::nullopt;
    }
};

template<>
struct TypeHelper<std::string>
{
    using OutputType = std::string;

    static std::optional<OutputType> get(::rapidjson::Value* jsonNode)
    {
        if (!jsonNode)
            return std::nullopt;
        if (jsonNode->IsString())
            return OutputType(jsonNode->GetString(), jsonNode->GetStringLength());
        if (jsonNode->IsInt())
            return std::to_string(jsonNode->GetInt());
        return std::nullopt;
    }
};

template<>
struct TypeHelper<QString>
{
    using OutputType = QString;

    static std::optional<OutputType> get(::rapidjson::Value* jsonNode)
    {
        auto value = TypeHelper<std::string>().get(jsonNode);
        if (value.has_value())
            return QString::fromStdString(value.value());
        return std::nullopt;
    }
};

template<>
struct TypeHelper<::rapidjson::Value*>
{
    using OutputType = ::rapidjson::Value*;

    static std::optional<OutputType> get(::rapidjson::Value* jsonNode)
    {
        if (!jsonNode)
            return std::nullopt;
        return jsonNode;
    }
};

enum class NX_UTILS_API ReturnCount
{
    One,
    All,
};

template<ReturnCount Count, typename T>
using OneOrMore = std::conditional_t<Count == ReturnCount::One, T, std::vector<T>>;

template<ReturnCount Count, typename T>
void addToRes(OneOrMore<Count, T>& res, T value)
{
    if constexpr (Count == ReturnCount::One)
        res = std::move(value);
    else
        res.push_back(std::move(value));
}

template<typename T>
concept ConvertedToRapidjsonValue = requires(T t)
{
    { ::rapidjson::Value{std::move(t)} };
};

template<typename T>
concept ConvertedToRapidjsonValueWithAllocator =
    requires(T t, ::rapidjson::Document::AllocatorType& alloc)
{
    { ::rapidjson::Value{std::move(t), alloc} };
};

struct NX_UTILS_API ValueHelper
{
    ::rapidjson::Document::AllocatorType& allocator;
    ::rapidjson::Value value;

    ValueHelper(const QString& val, ::rapidjson::Document::AllocatorType& alloc);

    template<class T>
    ValueHelper(T&& val, ::rapidjson::Document::AllocatorType& alloc): allocator(alloc)
    {
        if constexpr (std::is_same_v<T, ::rapidjson::Value*>)
        {
            // Deep copy rapidjson values. By default they are moved, leaving original value
            // nullified, but that can cause trouble:
            // - if copy, not move, was the intention
            // - they may come from another Processor, this will cause the lifetime issue.
            value = ::rapidjson::Value{std::move(*val), alloc};
        }
        else if constexpr (ConvertedToRapidjsonValue<T>)
        {
            value = ::rapidjson::Value{std::move(val)};
        }
        else if constexpr (ConvertedToRapidjsonValueWithAllocator<T>)
        {
            value = ::rapidjson::Value{std::move(val), alloc};
        }
        else
        {
            const std::string strVal = nx::reflect::json::serialize(val);
            ::rapidjson::Document doc(&allocator);
            doc.Parse(strVal);
            if (doc.IsObject())
                value = doc.GetObject();
            else if (doc.IsArray())
                value = doc.GetArray();
        }
    }

    ValueHelper(ValueHelper&&) = default;
    ValueHelper(const ValueHelper&) = delete;
    ValueHelper& operator=(ValueHelper&& valueHelper) noexcept;
    ValueHelper& operator=(const ValueHelper&) = delete;

    bool isNull() const;

    /**
     * Rapidjson uses move semantics for assignment, so:
     * - When the value is used once, we don't need to make a copy, and the return type has to be
     *     rapidjson::Value&.
     * - When the value is used in multiple cases, we should make a copy explicitly by creating a
     *     new rapidjson::Value using a constructor with an allocator.
     */
    template<ReturnCount Count>
    using ReturnValueType =
        std::conditional_t<Count == ReturnCount::One, ::rapidjson::Value&, ::rapidjson::Value>;

    template<ReturnCount Count>
    ReturnValueType<Count> get()
    {
        if constexpr (Count == ReturnCount::One)
            return value;
        else
            return ::rapidjson::Value{value, allocator};
    }
};

struct NX_UTILS_API Condition
{
    template<typename... Ts>
    struct Overload: Ts...
    {
        using Ts::operator()...;
    };

    template<class... Ts>
    Overload(Ts...) -> Overload<Ts...>;

    template<typename T>
    using Predicate = std::function<bool(const T&)>;

    Predicate<ObjectIterator> funcObj;
    Predicate<ArrayIterator> funcArr;

    template<PredicateType PredicateT>
    Condition(const PredicateT& pred);

    bool operator()(const ArrayIterator& it) const { return funcArr(it); }

    bool operator()(const ObjectIterator& it) const { return funcObj(it); }

    bool canApplyTo(::rapidjson::Value* v) const
    {
        return (funcObj && v->IsObject()) || (funcArr && v->IsArray());
    }
};

template<PredicateType PredicateT>
Condition::Condition(const PredicateT& pred)
{
    if constexpr (ObjectPredicateType<PredicateT>)
        funcObj = Overload{pred};
    if constexpr (ArrayPredicateType<PredicateT>)
        funcArr = Overload{pred};
}

template<RapidjsonIteratorType Type>
struct ValuePtrHelper
{
    using IteratorType = Type;

    ValuePtrHelper(::rapidjson::Value* v): m_ptr(v) {}

    bool isValid() const
    {
        if constexpr (IsObjectIterator<IteratorType>)
            return m_ptr && m_ptr->IsObject();
        else
            return m_ptr && m_ptr->IsArray();
    }

    IteratorType begin() const
    {
        if constexpr (IsObjectIterator<IteratorType>)
            return m_ptr->MemberBegin();
        else
            return m_ptr->Begin();
    }

    IteratorType end() const
    {
        if constexpr (IsObjectIterator<IteratorType>)
            return m_ptr->MemberEnd();
        else
            return m_ptr->End();
    }

    static ::rapidjson::Value* get(const IteratorType& it)
    {
        if constexpr (IsObjectIterator<IteratorType>)
            return &(it->value);
        else
            return it;
    }

    static ::rapidjson::Value* name(const IteratorType& it)
    {
        if constexpr (IsObjectIterator<IteratorType>)
            return &(it->name);
        else
            return nullptr;
    }

    IteratorType erase(const IteratorType& itr) const
    {
        if constexpr (IsObjectIterator<IteratorType>)
            return m_ptr->EraseMember(itr);
        else
            return m_ptr->Erase(itr);
    }

    static bool set(const IteratorType& itr, ::rapidjson::Value&& value)
    {
        if constexpr (IsObjectIterator<IteratorType>)
            itr->value = std::move(value);
        else
            *itr = std::move(value);
        return true;
    }

    bool add(::rapidjson::Value&& value, ::rapidjson::Document::AllocatorType& alloc) const
        requires(IsArrayIterator<IteratorType>)
    {
        if (!isValid())
            return false;
        m_ptr->PushBack(std::move(value), alloc);
        return true;
    }

    template<ElementWithExistingName AddingBehavior>
    bool add(::rapidjson::Value&& name,
        ::rapidjson::Value&& value,
        ::rapidjson::Document::AllocatorType& alloc) const
        requires(IsObjectIterator<IteratorType>)
    {
        if (!isValid())
            return false;

        if constexpr (AddingBehavior != ElementWithExistingName::add)
        {
            auto itr = m_ptr->FindMember(name.GetString());
            if (itr != end())
            {
                if constexpr (AddingBehavior == ElementWithExistingName::ignore)
                    return false;

                return set(itr, std::move(value)); //< Replace.
            }
        }

        m_ptr->AddMember(std::move(name), std::move(value), alloc);
        return true;
    }

private:
    ::rapidjson::Value* m_ptr;
};

using ObjectPtrHelper = ValuePtrHelper<ObjectIterator>;
using ArrayPtrHelper = ValuePtrHelper<ArrayIterator>;

template<ReturnCount Count>
class GetValue
{
public:
    static constexpr ReturnCount returnCount = Count;
    using ReturnType = OneOrMore<Count, ::rapidjson::Value*>;

    ReturnType result = {};

    template<RapidjsonIteratorType IteratorType>
    void execute(IteratorType& it, const ValuePtrHelper<IteratorType>& ptrHelper)
    {
        addToRes<returnCount>(result, ptrHelper.get(it));
        ++it;
    }

    void execute(const ::rapidjson::Pointer& path, ::rapidjson::Value* root)
    {
        addToRes<returnCount>(result, path.Get(*root));
    }
};

template<ReturnCount Count>
class EraseValue
{
public:
    static constexpr ReturnCount returnCount = Count;
    using ReturnType = OneOrMore<Count, bool>;

    ReturnType result = {};

    template<RapidjsonIteratorType IteratorType>
    void execute(IteratorType& it, const ValuePtrHelper<IteratorType>& ptrHelper)
    {
        it = ptrHelper.erase(it);
        addToRes<returnCount>(result, true);
    }

    void execute(const ::rapidjson::Pointer& path, ::rapidjson::Value* root)
    {
        addToRes<returnCount>(result, path.Erase(*root));
    }
};

template<ReturnCount Count>
class ModifyValue
{
public:
    static constexpr ReturnCount returnCount = Count;
    using ReturnType = OneOrMore<Count, bool>;

    ReturnType result = {};

    ModifyValue(ValueHelper&& val): m_val(std::move(val)) {}

    template<RapidjsonIteratorType IteratorType>
    void execute(IteratorType& it, const ValuePtrHelper<IteratorType>& ptrHelper)
    {

        addToRes<returnCount>(result, ptrHelper.set(it, std::move(m_val.get<returnCount>())));
        ++it;
    }

    void execute(const ::rapidjson::Pointer& path, ::rapidjson::Value* root)
    {
        ::rapidjson::Value* curr = path.Get(*root);
        if (!curr)
            return;
        *curr = std::move(m_val.get<returnCount>());
        addToRes<returnCount>(result, true);
    }

private:
    ValueHelper m_val;
};

template<ReturnCount Count>
class AddValueToArray
{
public:
    static constexpr ReturnCount returnCount = Count;
    using ReturnType = OneOrMore<Count, bool>;

    ReturnType result = {};

    AddValueToArray(ValueHelper&& val): m_val(std::move(val)) {}

    template<RapidjsonIteratorType IteratorType>
    void execute(IteratorType& it, const ValuePtrHelper<IteratorType>& ptrHelper)
    {
        ArrayPtrHelper curr{ptrHelper.get(it)};
        addToRes<returnCount>(result, curr.add(std::move(m_val.get<returnCount>()), m_val.allocator));
        ++it;
    }

    void execute(const ::rapidjson::Pointer& path, ::rapidjson::Value* root)
    {
        ArrayPtrHelper curr = path.Get(*root);
        addToRes<returnCount>(result, curr.add(std::move(m_val.get<returnCount>()), m_val.allocator));
    }

private:
    ValueHelper m_val;
};

template<ReturnCount Count, ElementWithExistingName AddingBehavior>
class AddValueToObject
{
public:
    static constexpr ReturnCount returnCount = Count;
    using ReturnType = OneOrMore<Count, bool>;

    ReturnType result = {};

    AddValueToObject(ValueHelper&& name, ValueHelper&& val): m_name(std::move(name)), m_val(std::move(val)) {}

    template<RapidjsonIteratorType IteratorType>
    void execute(IteratorType& it, const ValuePtrHelper<IteratorType>& ptrHelper)
    {
        ObjectPtrHelper curr{ptrHelper.get(it)};
        addToRes<returnCount>(
            result,
            curr.add<AddingBehavior>(
                std::move(m_name.get<returnCount>()),
                std::move(m_val.get<returnCount>()),
                m_val.allocator)
        );
        ++it;
    }

    void execute(const ::rapidjson::Pointer& path, ::rapidjson::Value* root)
    {
        ObjectPtrHelper curr = path.Get(*root);
        addToRes<returnCount>(
            result,
            curr.add<AddingBehavior>(
                std::move(m_name.get<returnCount>()),
                std::move(m_val.get<returnCount>()),
                m_val.allocator)
        );
    }

private:
    ValueHelper m_name;
    ValueHelper m_val;
};

template<typename T>
concept CustomActionFuncType = requires(T t, ::rapidjson::Value* name, ::rapidjson::Value* value)
{
    { t(name, value) } -> convertible_to<bool>;
};

template<ReturnCount Count>
class CustomAction
{
public:
    static constexpr ReturnCount returnCount = Count;
    using ReturnType = OneOrMore<Count, bool>;

    ReturnType result = {};

    template<CustomActionFuncType CustomActionFunc>
    CustomAction(const CustomActionFunc& action): m_customAction(action) {}

    template<RapidjsonIteratorType IteratorType>
    void execute(IteratorType& it, const ValuePtrHelper<IteratorType>& ptrHelper)
    {
        addToRes<returnCount>(result, m_customAction(ptrHelper.name(it), ptrHelper.get(it)));
        ++it;
    }

    void execute(const ::rapidjson::Pointer& path, ::rapidjson::Value* root)
    {
        ::rapidjson::Value* curr = path.Get(*root);
        if (curr)
            addToRes<returnCount>(result, m_customAction(nullptr, curr));
        else
            addToRes<returnCount>(result, false);
    }

private:
    std::function<bool(::rapidjson::Value*, ::rapidjson::Value*)> m_customAction;
};

class NX_UTILS_API Path
{
public:
    struct NX_UTILS_API Token
    {
        using Pointer = ::rapidjson::Pointer;
        std::variant<Condition, Pointer> token;

        bool isCondition() const noexcept { return std::holds_alternative<Condition>(token); }
        const Pointer& pointer() const noexcept { return std::get<Pointer>(token); }
        const Condition& condition() const noexcept { return std::get<Condition>(token); }
        bool isValid() const { return isCondition() || pointer().IsValid(); }

        template<PredicateType Predicate>
        Token(Predicate pred): token(std::move(pred)) {}
        Token(std::string_view stringToken): token(Pointer(stringToken.data(), stringToken.size())) {}
    };

public:
    Path(const Path& path) = default;

    template<PredicateType... Predicates>
    Path(std::string_view path, Predicates... preds)
    {
        m_tokens = std::make_shared<std::vector<Token>>();

        if (path.empty())
        {
            m_tokens->push_back(Token(""));
            return;
        }

        this->parse(path, preds...);
    }

    void parse(std::string_view path);

    template<PredicateType Predicate, PredicateType... Predicates>
    void parse(std::string_view path, Predicate pred, Predicates... preds)
    {
        auto predPos = path.find(conditionToken);
        if (predPos == std::string_view::npos)
        {
            m_valid = false;
            return;
        }

        if (predPos > 0)
        {
            if (Token token{path.substr(0, predPos)}; token.isValid())
            {
                m_tokens->push_back(std::move(token));
            }
            else
            {
                m_valid = false;
                return;
            }
        }
        m_tokens->push_back(std::move(pred));
        parse(path.substr(predPos + 4), preds...);
    }

    int tokenCount() const;
    bool isValid() const;

    int conditionCount() const;

    Path next() const;
    bool hasNext() const;

    const Token& currentToken() const;

    int getPos() const { return m_pos; }

private:
    static constexpr std::string_view conditionToken = "/[?]";
    std::shared_ptr<std::vector<Token>> m_tokens;
    bool m_valid = true;
    int m_pos = 0;
};

} // namespace details

namespace predicate {

class NX_UTILS_API All
{
public:
    bool operator()(const auto&) const
    {
        return true;
    }
};

class NX_UTILS_API NameContains
{
public:
    explicit NameContains(std::string_view namePart): m_namePart(namePart) {}

    bool operator()(const ObjectIterator& root) const;

private:
    std::string m_namePart;
};

class NX_UTILS_API HasMember
{
public:
    explicit HasMember(std::string_view name): m_name(name) {}

    bool operator()(const ArrayIterator& root) const
    {
        return root->IsObject() && root->GetObject().HasMember(m_name);
    }

private:
    std::string m_name;
};

class NX_UTILS_API Compare
{
public:
    explicit Compare(std::string_view key, const QString& val);

    bool operator()(const ArrayIterator& root) const;

private:
    const ::rapidjson::Pointer m_pointer;
    const QString m_value;
};

class NX_UTILS_API ContainedIn
{
public:
    explicit ContainedIn(std::string_view key, const QStringList& values);

    bool operator()(const ArrayIterator& root) const;

private:
    const ::rapidjson::Pointer m_pointer;
    const QStringList m_values;
};

class NX_UTILS_API IsEmpty
{
public:
    bool operator()(const ArrayIterator& root) const
    {
        return root->Empty();
    }

    bool operator()(const ObjectIterator& root) const
    {
        return root->value.ObjectEmpty();
    }
};

} // namespace predicate

template<typename T>
concept ActionType = requires(T t, const ::rapidjson::Pointer& path, ::rapidjson::Value* root)
{
    { t.execute(path, root) };
    { t.result } -> convertible_to<typename T::ReturnType>;
};

class NX_UTILS_API Processor
{
public:
    explicit Processor(ConstBufferRefType data);
    Processor(const Processor& queryProcessor) = default;

    template<PredicateType Predicate>
    std::vector<Processor> find(Predicate pred) const
    {
        std::vector<Processor> outputVector;
        details::Condition cond{pred};

        details::GetValue<details::ReturnCount::All> action;
        getWithRecursiveSearch(cond, m_curr, &action);

        for (auto& val: action.result)
        {
            if (val)
                outputVector.push_back(Processor(*this, val));
        }
        return outputVector;
    }

    template<class T, PredicateType... Predicates>
    std::optional<T> getValue(std::string_view path, Predicates... preds) const
    {
        details::Path parsedPath{path, preds...};
        if (!parsedPath.isValid())
            return std::nullopt;

        details::GetValue<details::ReturnCount::One> action;

        applyActionWithCondition(parsedPath, m_curr, &action);

        if (!action.result)
            return std::nullopt;
        if constexpr (std::is_same_v<T, Processor>)
            return Processor(*this, action.result);
        else
            return details::TypeHelper<T>::get(action.result);
    }

    template<class T, PredicateType... Predicates>
    std::vector<T> getAllValues(std::string_view path, Predicates... preds) const
    {
        std::vector<T> outputVector;
        const details::Path parsedPath{path, preds...};
        if (!parsedPath.isValid())
            return outputVector;

        details::GetValue<details::ReturnCount::All> action;
        applyActionWithCondition(parsedPath, m_curr, &action);

        for (auto& val: action.result)
        {
            if constexpr (std::is_same_v<T, Processor>)
            {
                if (val)
                    outputVector.push_back(Processor(*this, val));
            }
            else
            {
                auto convVal = details::TypeHelper<T>::get(val);
                if (convVal.has_value())
                    outputVector.push_back(convVal.value());
            }
        }
        return outputVector;
    }

    template<PredicateType... Predicates>
    bool eraseValue(std::string_view path, Predicates... preds)
    {
        details::Path parsedPath{path, preds...};
        if (!parsedPath.isValid())
            return false;
        details::EraseValue<details::ReturnCount::One> action;
        applyActionWithCondition(parsedPath, m_curr, &action);
        return action.result;
    }

    template<PredicateType... Predicates>
    int eraseAllValues(std::string_view path, Predicates... preds)
    {
        details::Path parsedPath{path, preds...};
        if (!parsedPath.isValid())
            return 0;
        details::EraseValue<details::ReturnCount::All> action;
        applyActionWithCondition(parsedPath, m_curr, &action);
        return std::count_if(action.result.begin(), action.result.end(), [](const auto& t) { return t; });
    }

    template<class T, PredicateType... Predicates>
    bool modifyValue(T&& newValue, std::string_view path, Predicates... preds)
    {
        details::Path parsedPath{path, preds...};
        if (!parsedPath.isValid())
            return false;
        details::ModifyValue<details::ReturnCount::One> action{
            details::ValueHelper(std::move(newValue), m_data->GetAllocator())};
        applyActionWithCondition(parsedPath, m_curr, &action);
        return action.result;
    }

    template<class T, PredicateType... Predicates>
    int modifyAllValues(T&& newValue,std::string_view path, Predicates... preds)
    {
        details::Path parsedPath{path, preds...};
        if (!parsedPath.isValid())
            return 0;
        details::ModifyValue<details::ReturnCount::All> action{
            details::ValueHelper(std::move(newValue), m_data->GetAllocator())};
        applyActionWithCondition(parsedPath, m_curr, &action);
        return std::count_if(action.result.begin(), action.result.end(),
            [](const auto& t) { return t; });
    }

    template<class T, PredicateType... Predicates>
    bool addValueToArray(T&& newValue,std::string_view path, Predicates... preds)
    {
        details::Path parsedPath{path, preds...};
        if (!parsedPath.isValid())
            return false;
        details::AddValueToArray<details::ReturnCount::One> action{
            details::ValueHelper(std::move(newValue), m_data->GetAllocator())};
        applyActionWithCondition(parsedPath, m_curr, &action);
        return action.result;
    }

    template<class T, PredicateType... Predicates>
    int addValueToArrayAll(T&& newValue,std::string_view path, Predicates... preds)
    {
        details::Path parsedPath{path, preds...};
        if (!parsedPath.isValid())
            return 0;
        details::AddValueToArray<details::ReturnCount::All> action{
            details::ValueHelper(std::move(newValue), m_data->GetAllocator())};
        applyActionWithCondition(parsedPath, m_curr, &action);
        return std::count_if(action.result.begin(), action.result.end(),
            [](const auto& t) { return t; });
    }

    /*
     * RFC 8259 ( https://www.rfc-editor.org/rfc/rfc8259#section-4 ) says: "The names within an
     * object SHOULD be unique.". So it is recommended to use unique names in the object, but it
     * is not obligatory. Due to this uncertainty, addValueToObject can behave in several different
     * manners:
     *   - add     : the new element will be added even if an element with the same name already
     *               exists (equals to rapidjson AddMember behavior)
     *   - replace : (default option) the existing element value with the same name will be
     *               replaced with the new one
     *   - ignore  : if the element with the same name already exists, no changes will be made,
     *               function will return false
     */
    template<
        ElementWithExistingName AddingBehavior = ElementWithExistingName::replace,
        class T,
        PredicateType... Predicates
    >
    bool addValueToObject(
        const std::string& name, T&& newValue,std::string_view path, Predicates... preds)
    {
        details::Path parsedPath{path, preds...};
        if (!parsedPath.isValid())
            return false;
        details::AddValueToObject<details::ReturnCount::One, AddingBehavior> action{
            details::ValueHelper(name, m_data->GetAllocator()),
            details::ValueHelper(std::move(newValue), m_data->GetAllocator())};
        applyActionWithCondition(parsedPath, m_curr, &action);
        return action.result;
    }

    template<
        ElementWithExistingName AddingBehavior = ElementWithExistingName::replace,
        class T,
        PredicateType... Predicates
    >
    int addValueToObjectAll(
        const std::string& name, T&& newValue,std::string_view path, Predicates... preds)
    {
        details::Path parsedPath{path, preds...};
        if (!parsedPath.isValid())
            return 0;
        details::AddValueToObject<details::ReturnCount::All, AddingBehavior> action{
            details::ValueHelper(name, m_data->GetAllocator()),
            details::ValueHelper(std::move(newValue), m_data->GetAllocator())};
        applyActionWithCondition(parsedPath, m_curr, &action);
        return std::count_if(action.result.begin(), action.result.end(),
            [](const auto& t) { return t; });
    }

    template<details::CustomActionFuncType CustomActionFunc, PredicateType... Predicates>
    bool applyActionToValue(
        const CustomActionFunc& customAction,std::string_view path, Predicates... preds)
    {
        details::Path parsedPath{path, preds...};
        if (!parsedPath.isValid())
            return false;
        details::CustomAction<details::ReturnCount::One> action{customAction};
        applyActionWithCondition(parsedPath, m_curr, &action);
        return action.result;
    }

    template<details::CustomActionFuncType CustomActionFunc, PredicateType... Predicates>
    int applyActionToValues(
        const CustomActionFunc& customAction,std::string_view path, Predicates... preds)
    {
        details::Path parsedPath{path, preds...};
        if (!parsedPath.isValid())
            return 0;
        details::CustomAction<details::ReturnCount::All> action{customAction};
        applyActionWithCondition(parsedPath, m_curr, &action);
        return std::count_if(action.result.begin(), action.result.end(),
            [](const auto& t) { return t; });
    }

    std::string toStdString() const;
    bool isValid() const;
    ::rapidjson::ParseErrorCode getParseError() const;

private:
    Processor(const Processor& processor, ::rapidjson::Value* newCurr):
        m_data(processor.m_data), m_curr(newCurr)
    {
    }

    template<typename PtrHelperType, ActionType Action>
    void iterate(
        details::Path path,
        ::rapidjson::Value* parent,
        const details::Condition& condition,
        Action* action) const
    {
        PtrHelperType ptrHelper(parent);
        const bool hasNext = path.hasNext();
        const auto next = path.next();

        for (auto it = ptrHelper.begin(); it != ptrHelper.end();)
        {
            if (condition(it))
            {
                if (hasNext)
                {
                    applyActionWithCondition(next, ptrHelper.get(it), action);
                    ++it;
                }
                else
                {
                    action->execute(it, ptrHelper);
                }

                if constexpr (Action::returnCount == details::ReturnCount::One)
                    return;
            }
            else
            {
                ++it;
            }
        }
    }

    template<ActionType Action>
    void applyActionWithCondition(
        details::Path path,
        ::rapidjson::Value* parent,
        Action* action) const
    {
        if (!parent)
            return;

        const auto& currentToken = path.currentToken();

        if (currentToken.isCondition())
        {
            const auto& cond = currentToken.condition();
            if (!cond.canApplyTo(parent))
                return;

            if (parent->IsObject())
                iterate<details::ObjectPtrHelper>(path, parent, cond, action);
            else if (parent->IsArray())
                iterate<details::ArrayPtrHelper>(path, parent, cond, action);
            else
                return;
        }
        else
        {
            if (path.hasNext())
                applyActionWithCondition(path.next(), currentToken.pointer().Get(*parent), action);
            else
                action->execute(currentToken.pointer(), parent);
        }
    }

    template<typename PtrHelperType>
    void iterateRecursive(
        ::rapidjson::Value* parent,
        const details::Condition& condition,
        details::GetValue<details::ReturnCount::All>* action) const
    {
        PtrHelperType ptrHelper(parent);
        for (auto it = ptrHelper.begin(); it != ptrHelper.end();)
        {
            if (condition.canApplyTo(parent) && condition(it))
            {
                action->execute(it, ptrHelper);
            }
            else
            {
                getWithRecursiveSearch(condition, ptrHelper.get(it), action);
                ++it;
            }
        }
    }

    void getWithRecursiveSearch(
        const details::Condition& condition,
        ::rapidjson::Value* parent,
        details::GetValue<details::ReturnCount::All>* action) const;

private:
    std::shared_ptr<::rapidjson::Document> m_data;
    ::rapidjson::Value* m_curr = nullptr;
};

} // namespace nx::utils::rapidjson
