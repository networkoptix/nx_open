// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_EMAIL_FWD_H
#define QN_EMAIL_FWD_H

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <nx/vms/api/types/smtp_types.h>

namespace QnEmail {

using ConnectionType = nx::vms::api::ConnectionType;

} // namespace QnEmail

struct QnEmailSmtpServerPreset;
struct QnEmailSettings;
class  QnEmailAddress;

enum SmtpReplyCode
{
    NoReply = 0,
    NonStandardSuccess = 200,
    SystemStatus = 211,
    Help = 214,
    ServiceReady = 220,
    ServiceClosedChannel = 221,
    AuthSuccessful = 235,
    MailActionOK = 250,
    UserNotLocal = 251,
    CannotVerifyUser = 252,
    ServerChallenge = 334,
    StartMailInput = 354,
    ServiceNotAvailable = 421,
    MailboxTemporaryUnavailable = 450,
    ProcessingError = 451,
    InsufficientStorage = 452,
    CommandSyntaxError = 500,
    ParameterSyntaxError = 501,
    CommandNotImplemented = 502,
    BadCommandSequence = 503,
    ParameterNotImplemented = 504,
    MailIsNotAccepted = 521,
    AccessDenied = 530,
    MailboxUnavailable = 550,
    UserNotLocalError = 551,
    ActionAborted = 552,
    InvalidMailboxName = 553,
    TransactionFailed = 554,
};

namespace nx::email { enum class SmtpError; }

struct SmtpOperationResult;

#endif // QN_EMAIL_FWD_H
