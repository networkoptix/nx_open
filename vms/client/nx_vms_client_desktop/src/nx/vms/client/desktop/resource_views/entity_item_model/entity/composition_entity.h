// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_entity.h"

namespace nx::vms::client::desktop {
namespace entity_item_model {

/**
 * @todo UniqueKeyCompositionEntity which allow index -> entity (enum -> entity) mapping and may
 *     be used to create sparse arrays will fit better for every case now.
 *
 * Entity which doesn't hold items itself, but aggregates other entities. There is no
 * limitations on its contents, composites of composites is may be used as well, but it's not
 * the best choice for homogeneous lists or entities which may be parameterized somehow. One of
 * the important properties of composite entity is the ability to explicitly keep the order of its
 * sub-entities, i.e define grouping.
 */
class NX_VMS_CLIENT_DESKTOP_API CompositionEntity: public AbstractEntity
{
    using base_type = AbstractEntity;

public:
    CompositionEntity();
    virtual ~CompositionEntity() override;

    void addSubEntity(AbstractEntityPtr subEntity);
    void removeSubEntity(const AbstractEntity* subEntity);

    int subEntityCount() const;
    AbstractEntity* subEntityAt(int index);
    AbstractEntity* lastSubentity();

    virtual int rowCount() const override;
    virtual AbstractEntity* childEntity(int row) const override;
    virtual int childEntityRow(const AbstractEntity* entity) const override;

    virtual QVariant data(int row, int role = Qt::DisplayRole) const override;
    virtual Qt::ItemFlags flags(int row) const override;

    virtual bool isPlanar() const override;

    virtual void hide() override;
    virtual void show() override;

private:
    int subEntityOffset(const AbstractEntity* subEntity) const;
    std::pair<AbstractEntity*, int> subEntityWithLocalRow(int row) const;

private:
    std::vector<AbstractEntityPtr> m_composition;
    bool m_isHidden = false;
    int m_hiddenRowsCount = 0;
};

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
