#pragma once

#include <string>
#include <vector>

#include <nx/clusterdb/engine/command_descriptor.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx::clusterdb::engine::test {

struct Id
{
    std::string id;
};

#define Id_Fields (id)

struct Customer
{
    std::string id;
    std::string fullName;
    std::string address;

    bool operator==(const Customer& right) const
    {
        return id == right.id
            && fullName == right.fullName
            && address == right.address;
    }
};

#define Customer_Fields (id)(fullName)(address)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Id)(Customer),
    (ubjson)(json))

using Customers = std::vector<Customer>;

//-------------------------------------------------------------------------------------------------

namespace command {

enum Code
{
    saveCustomer = nx::clusterdb::engine::command::kUserCommand + 1,
    removeCustomer,
};

struct SaveCustomer
{
    using Data = Customer;
    static constexpr int code = Code::saveCustomer;
    static constexpr char name[] = "saveCustomer";

    static nx::Buffer hash(const Data& data)
    {
        return data.id.c_str();
    }
};

struct RemoveCustomer
{
    using Data = Id;
    static constexpr int code = Code::removeCustomer;
    static constexpr char name[] = "removeCustomer";

    static nx::Buffer hash(const Data& data)
    {
        return data.id.c_str();
    }
};

} // namespace command

} // namespace nx::clusterdb::engine::test
