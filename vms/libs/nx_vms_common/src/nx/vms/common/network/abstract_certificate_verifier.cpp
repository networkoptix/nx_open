// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_certificate_verifier.h"

namespace nx::vms::common {

AbstractCertificateVerifier::AbstractCertificateVerifier(QObject* parent): QObject(parent) {}
AbstractCertificateVerifier::~AbstractCertificateVerifier() {}

} // nx::vms::common
