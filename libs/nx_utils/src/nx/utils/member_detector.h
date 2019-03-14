#pragma once

#include <type_traits>

#include <nx/utils/signature_extractor.h>

#define NX_UTILS_DECLARE_METHOD_DETECTOR(NAME, METHOD_NAME, SIGNATURE) \
    template<typename T, typename = void> \
    struct NAME: std::false_type {}; \
    \
    template<typename T> \
    struct NAME< \
        T, \
        std::void_t< \
            std::enable_if_t< \
                std::is_same_v< \
                    typename nx::utils::meta::SignatureExtractor<decltype(&T::METHOD_NAME)>::type, \
                    SIGNATURE \
                > \
            > \
        > \
    >: std::true_type {}

#define NX_UTILS_DECLARE_FIELD_DETECTOR(NAME, FIELD_NAME, SIGNATURE) \
    template<typename T, typename = void> \
    struct NAME: std::false_type {}; \
    \
    template<typename T> \
    struct NAME< \
        T, \
        std::void_t< \
            std::enable_if_t< \
                std::is_same_v< \
                    decltype(T::FIELD_NAME), \
                    SIGNATURE \
                > \
            > \
        > \
    >: std::true_type {}

#define NX_UTILS_DECLARE_FIELD_DETECTOR_SIMPLE(NAME, FIELD_NAME) \
    template<typename T, typename = void> \
    struct NAME: std::false_type {}; \
    \
    template <typename T> \
    struct NAME< \
        T, \
        std::void_t<decltype(&T::FIELD_NAME)> \
    >: std::true_type {}
