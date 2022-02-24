// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_entity.h"
#include <nx/utils/log/assert.h>
#include <client/client_globals.h>
#include <unordered_map>
#include <vector>
#include <numeric>

namespace nx::vms::client::desktop {
namespace entity_item_model {

template <class Key>
class NX_VMS_CLIENT_DESKTOP_API UniqueKeyCompositionEntity: public AbstractEntity
{
    using base_type = AbstractEntity;

public:
    UniqueKeyCompositionEntity();
    virtual ~UniqueKeyCompositionEntity() override;

    virtual int rowCount() const override;
    virtual AbstractEntity* childEntity(int row) const override;
    virtual int childEntityRow(const AbstractEntity* entity) const override;
    virtual QVariant data(int row, int role = Qt::DisplayRole) const override;
    virtual Qt::ItemFlags flags(int row) const override;
    virtual bool isPlanar() const override;

    AbstractEntity* subEntity(const Key& key);
    void setSubEntity(const Key& key, AbstractEntityPtr subEntityPtr);
    bool removeSubEntity(const Key& key);

private:
    int subEntityIndex(const Key& key) const;
    int subEntityOffset(const AbstractEntity* subEntity) const;
    std::pair<AbstractEntity*, int> subEntityWithLocalRow(int row) const;

private:
    struct KeyEntityPair
    {
        KeyEntityPair(Key keyParam): key(keyParam) {};
        KeyEntityPair(Key keyParam, AbstractEntityPtr entityParam):
            key(keyParam),
            entity(std::move(entityParam)) {};
        bool operator<(const KeyEntityPair& other) const { return key < other.key; };

        Key key;
        AbstractEntityPtr entity;
    };

    std::vector<KeyEntityPair> m_subEntities;
};

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
