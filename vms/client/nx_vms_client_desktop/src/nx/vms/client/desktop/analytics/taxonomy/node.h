// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include "abstract_node.h"

#include <nx/utils/impl_ptr.h>

namespace nx::analytics::taxonomy { class AbstractObjectType; }

namespace nx::vms::client::desktop::analytics::taxonomy {

class AbstractStateViewFilter;

class Node: public AbstractNode
{
public:
    Node(const nx::analytics::taxonomy::AbstractObjectType* mainObjectType,
        QObject* parent = nullptr);

    virtual ~Node() override;

    virtual std::vector<QString> typeIds() const override;

    virtual std::vector<QString> fullSubtreeTypeIds() const override;

    virtual QString name() const override;

    virtual QString icon() const override;

    virtual std::vector<AbstractAttribute*> attributes() const override;

    virtual std::vector<AbstractNode*> derivedNodes() const override;

    void addObjectType(const nx::analytics::taxonomy::AbstractObjectType* objectType);

    void addDerivedNode(AbstractNode* node);

    void setFilter(const AbstractStateViewFilter* filter);

    void resolveAttributes();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
