// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_data_receptor.h"

#include <nx/utils/log/log.h>

QnAbstractDataReceptor::~QnAbstractDataReceptor()
{
    NX_ASSERT(consumers.load() == 0, consumers.load());
}
