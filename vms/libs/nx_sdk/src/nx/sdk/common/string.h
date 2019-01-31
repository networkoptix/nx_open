#include <nx/sdk/i_string.h>

#include <plugins/plugin_tools.h>

namespace nx {
namespace sdk {
namespace common {

class String: public nxpt::CommonRefCounter<IString>
{
public:
    String() = default;
    String(std::string s);

    /** @param s If null, empty string is assumed. */
    String(const char* s);

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual const char* str() const override;

    void setString(std::string s);

    /** @param s If null, empty string is assumed. */
    void setString(const char* s);

    int size() const;

    bool empty() const;

private:
    std::string m_string;
};

} // namespace common
} // namespace sdk
} // namespace nx
