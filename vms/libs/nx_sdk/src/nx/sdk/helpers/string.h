#include <nx/sdk/i_string.h>

#include <nx/sdk/helpers/ref_countable.h>

namespace nx {
namespace sdk {

class String: public RefCountable<IString>
{
public:
    String() = default;
    String(std::string s);

    /** @param s If null, empty string is assumed. */
    String(const char* s);

    virtual const char* str() const override;

    void setString(std::string s);

    /** @param s If null, empty string is assumed. */
    void setString(const char* s);

    int size() const;

    bool empty() const;

private:
    std::string m_string;
};

} // namespace sdk
} // namespace nx
