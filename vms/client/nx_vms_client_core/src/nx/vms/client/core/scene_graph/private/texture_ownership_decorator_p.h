// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::client::core::sg {

template<typename BaseClass, typename... ConstructorArgs>
class TextureOwnershipDecorator: public BaseClass
{
public:
    explicit TextureOwnershipDecorator(ConstructorArgs... args, bool ownsTexture = true):
        BaseClass(std::forward<ConstructorArgs>(args)...),
        m_ownsTexture(ownsTexture)
    {
    }

    virtual ~TextureOwnershipDecorator() override
    {
        if (m_ownsTexture)
            delete this->texture();
    }

    bool ownsTexture() const { return m_ownsTexture; }
    void setOwnsTexture(bool value) { m_ownsTexture = value; }

    void resetTexture(QSGTexture* value)
    {
        if (this->texture() == value)
            return;

        if (m_ownsTexture)
            delete this->texture();

        setTexture(value);
    }

protected:
    using BaseClass::setTexture;

private:
    bool m_ownsTexture = true;
};

} // namespace nx::vms::client::core::sg
