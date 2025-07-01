// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include "../rules_fwd.h"
#include "event_parameter_helper.h"

class SystemContext;

namespace nx::vms::rules::utils {

enum class TextTokenType
{
    text,
    substitution
};

struct TextToken
{
    TextTokenType type = TextTokenType::text;
    QString value;
    bool isValid = true;
    qsizetype start = 0;
    qsizetype length = 0;
};

using TextTokenList = QList<TextToken>;

/**
 * This function gets a template string, containing event parameters in curly braces, and returns
 * a list of tokens of two types: "text" and "substitution". The text token contains the text
 * between the event parameters, and the substitution token contains the name of the event
 * parameter.
 */
TextTokenList tokenizeText(QString text);

/**
 * This function takes a list of tokens and returns a string, replacing the substitution tokens
 * with the corresponding values of the event parameters.
 */
QString composeTextFromTokenList(
    const TextTokenList& tokens,
    common::SystemContext* systemContext,
    const AggregatedEventPtr& event);

} // namespace nx::vms::rules::utils
