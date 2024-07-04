// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <type_traits>

#include <nx/utils/member_detector.h>
#include <nx/utils/type_traits.h>
#include <nx/utils/uuid.h>

namespace nx::utils::model {

namespace detail {

#define MEMBER_CHECKER(MEMBER) \
    NX_UTILS_DECLARE_FIELD_DETECTOR_SIMPLE(DoesMemberExist_##MEMBER, MEMBER)
    MEMBER_CHECKER(getId);
    MEMBER_CHECKER(id);
#undef MEMBER_CHECKER

template<typename T, typename Arg, typename = void>
struct CanSetIdWithArg: std::false_type {};

template<typename T, typename Arg>
struct CanSetIdWithArg<T,
    Arg,
    std::void_t<decltype(std::declval<T&>().setId(std::declval<Arg>()))>>: std::true_type
{
};

template<typename T, typename Arg, typename = void>
struct CanAssignIdField: std::false_type {};

template<typename T, typename Arg>
struct CanAssignIdField<T,
    Arg,
    std::void_t<decltype(std::declval<T&>().id = std::move(std::declval<Arg>()))>>:
    std::true_type
{
};

} // namespace detail

/**
 * Customization point for use in generic template functions.
 * It allowes to simplify Crud Data type declarations:
 * it allows to simply specify `.id` member, or declare `getId() const` member function that will
 * be used to disambiguate the id, if there are multiple id-like members. `getId()` has a higher
 * priority.
 *
 * User can provide his own overload of `getId(CustomType)` that will be selected via ADL.
 * When writing generic code, a using declaration must be used to allow the ADL:
 * ```
 * template <typename T>
 * void SomeGenericFunction(const T& model)
 * {
 *     using nx::utils::model::getId;
 *     const auto id = getId(model);
 * }
 * ```
 */
template<typename T,
    typename = std::enable_if_t<detail::DoesMemberExist_getId<T>::value
        || detail::DoesMemberExist_id<T>::value || utils::IsKeyValueContainer<T>()>>
decltype(auto) getId(const T& t)
{
    if constexpr (detail::DoesMemberExist_getId<T>::value)
        return t.getId();
    else if constexpr (detail::DoesMemberExist_id<T>::value)
        return t.id;
    else
    {
        static_assert(!sizeof(T), "Failed to determine the id: no `id` or `getId` members");
    }
}

/**
 * Customization point for use in generic template functions.
 * It allowes to simplify Crud Data type declarations:
 * it allows to simply specify `.id` member, or declare `setId(Model, Id)` member function that
 * will be used to disambiguate the id, if there are multiple id-like members. `setId()` has a
 * higher priority.
 *
 * User can provide his own overload of `setId(CustomType)` that will be selected via ADL.
 * When writing generic code, a using declaration must be used to allow the ADL:
 * ```
 * template <typename T, typename Id>
 * void SomeGenericFunction(T& model, Id&& id)
 * {
 *     using nx::utils::model::setId;
 *     setId(model, std::forward<Id>(id));
 * }
 * ```
 */
template<typename T,
    typename Id,
    typename Expect = std::enable_if_t<
        std::disjunction<detail::CanSetIdWithArg<T, Id>,
            detail::CanAssignIdField<T, Id>>::value>>
void setId(T& t, Id&& id)
{
    if constexpr (detail::CanSetIdWithArg<T, Id>::value)
        t.setId(std::forward<Id>(id));
    else if constexpr (detail::CanAssignIdField<T, Id>::value)
        t.id = std::forward<Id>(id);
    else
        static_assert(!sizeof(T), "Failed to determine the id: no `id` or `setId` members");
}

template<typename, typename = std::void_t<>>
struct HasGetId: std::false_type {};
template<typename T>
struct HasGetId<T, std::void_t<decltype(getId(std::declval<T>()))>>: std::true_type {};

template<typename, typename, typename = std::void_t<>>
struct CanSetIdWithArg: std::false_type {};
template<typename T, typename Arg>
struct CanSetIdWithArg<T, Arg, std::void_t<decltype(setId(std::declval<T&>(), std::declval<Arg>()))>>:
    std::true_type
{
};

/**
 * Customisation point to enable/disable model's Id generation by CrudHandler upon a POST request.
 *
 * By default CrudHandler's implementation determines the possibility to generate Id using
 * `HasGetId<Model> && CanSetIdWithArg<Model, nx::Uuid>`, which can:
 * - falsely evaluate to `true` for some Models, resulting in compilation errors
 * - correctly evaluating to `true`, but `create` method is used for a procedure call and id
 * generation is not desired
 * User can provide an overload that returns `std::false` for a specific Model to disable id generation.
 * Example:
 * ```
 * namespace nx::vms::api {
 *
 * struct ProcedureCallRequest
 * {
 *    // Generation would be enabled by default, but this is not a Crud model
 *    nx::Uuid id;
 * };
 *
 * // This will disable the generation logic. This function will never be invoked at runtime.
 * std::false_type enableIdGeneration(ProcedureCallRequest);
 *
 * } // namespace nx::vms::api
 * ```
 */
template<typename Model>
constexpr auto enableIdGeneration(const Model&) -> std::conjunction<HasGetId<Model>, CanSetIdWithArg<Model, nx::Uuid>>{};

template<typename Model>
constexpr bool isIdGenerationEnabled = decltype(enableIdGeneration(std::declval<Model>()))::value;

/**
 * Ad hoc customization point to preserve the old behavior of `CrudHandler::executeDelete`:
 * It used to check if `model.getId() == decltype(model.getId()){}`.
 * Since `getId()` has been removed from many models and `getId(model)` can pickup `model.id`,
 * the check might falsely succeed.
 * The default behavior yields true if `model.getId()` is a valid expression.
 * Example:
 * ```
 * namespace nx::vms::api {
 *
 * struct UserDefinedModel
 * {
 *    // `id` can be a "*", and will be initialized to `nx::Uuig()` resulting in `missingParameter`
 *    nx::Uuid id;
 * };
 *
 * // This will disable the id check in `executeDelete`. This function will never be invoked at runtime.
 * std::false_type enableIdGeneration(UserDefinedModel);
 *
 * } // namespace nx::vms::api
 * ```
 */
template <typename Model>
constexpr auto adHocEnableIdCheckOnDelete(const Model&) -> detail::DoesMemberExist_getId<Model>;

template <typename Model>
constexpr bool adHocIsIdCheckOnDeleteEnabled =
    decltype(adHocEnableIdCheckOnDelete(std::declval<Model>()))::value;

} // namespace nx::utils::model
