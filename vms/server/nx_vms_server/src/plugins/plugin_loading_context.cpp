#include "plugin_loading_context.h"

#if defined(_WIN32)
    #include <WinBase.h>
#endif

#include <nx/kit/utils.h>
#include <nx/utils/system_error.h>

#include "vms_server_plugins_ini.h"
#include "plugin_manager.h" //< Usage of PluginManager as a logging tag requires class definition.

class PluginLoadingContextImpl
{
public:
    /**
     * @param pluginHomeDir If empty, the plugin is assumed to reside in a dir with other plugins.
     */
    PluginLoadingContextImpl(
        const PluginManager* pluginManager, QString pluginHomeDir, QString libName)
        :
        m_pluginManager(pluginManager),
        m_pluginHomeDir(std::move(pluginHomeDir)),
        m_libName(std::move(libName))
    {
        #if defined(_WIN32)
            const QString originalDllDirectory = getDllDirectory("before loading");
            // Can be null on error - ignoring the error and assuming an empty string.
            m_originalDllDirectory = originalDllDirectory.isNull() ? "" : originalDllDirectory;

            if (!pluginsIni().disablePluginLinkedDllLookup && !m_pluginHomeDir.isEmpty())
            {
                setDllDirectory(m_pluginHomeDir, "before loading");
            }
            else
            {
                // For plugins residing in a dir with other plugins, set the default DLL search
                // order excluding the current dir.
                setDllDirectory("", "before loading");
            }
        #endif
    }

    ~PluginLoadingContextImpl()
    {
        #if defined(_WIN32)
            // Always restore the default DLL search order (which includes the current dir).
            setDllDirectory(
                m_originalDllDirectory.isEmpty() ? QString::null : m_originalDllDirectory,
                "after loading");
        #endif
    }

#if defined(_WIN32)

private:
    /**
     * @return The current value (possibly empty) or null QString on error. Note: there seems to be
     *     no way to learn whether the previous call to SetDllDirectoryW() has set null or empty
     *     string.
     */
    QString getDllDirectory(const char* logNote) const
    {
        const QString logPrefix = lm("(%1 plugin %2)").args(logNote, m_libName);

        const DWORD len = GetDllDirectoryW(0, nullptr);
        const auto errorCode = SystemError::getLastOSErrorCode();
        if (len == 0 && errorCode != SystemError::noError)
        {
            NX_ERROR(m_pluginManager, "%1 GetDllDirectoryW(0, nullptr) -> %2: Error %3: %4",
                logPrefix, len, errorCode, SystemError::toString(errorCode));
            return QString::null;
        }
        NX_DEBUG(m_pluginManager, "%1 GetDllDirectoryW(0, nullptr) -> %2", logPrefix, len);

        std::vector<wchar_t> result(len);
        const DWORD r = GetDllDirectoryW(len, result.data());
        const auto e = SystemError::getLastOSErrorCode();
        if (r == 0 && e != SystemError::noError)
        {
            NX_ERROR(m_pluginManager, "%1 GetDllDirectoryW(%2, result) -> %3: Error %4: %5",
                logPrefix, len, r, errorCode, SystemError::toString(errorCode));
            return QString::null;
        }
        NX_DEBUG(m_pluginManager, "%1 GetDllDirectoryW(%2, result) -> %3 %4",
            logPrefix, len, r, nx::kit::utils::toString(result.data()));
        return QString::fromUtf16((const char16_t*) result.data());
    }

    /**
     * @param value Distinguishes null and empty string - null sets the default DLL search order,
     *     and empty string sets the default DLL search order without the current dir.
     */
    void setDllDirectory(const QString& value, const char* logNote) const
    {
        const QString messageFmt = lm("(%1 plugin %2) SetDllDirectoryW(%3) %4").args(
            logNote,
            m_libName,
            // Enquoted and escaped representation of the UTF-16 string for logging.
            value.isNull() ? "null" : nx::kit::utils::toString((const wchar_t*) value.utf16()),
            // Placeholder for the call result description.
            "%1"
        );

        const LPCWSTR ptr = value.isNull() ? nullptr : ((LPCWSTR) value.utf16());
        if (!SetDllDirectoryW(ptr))
            NX_ERROR(m_pluginManager, messageFmt, "failed: " + SystemError::getLastOSErrorText());
        else
            NX_DEBUG(m_pluginManager, messageFmt, "succeeded");
    }

#endif // defined(_WIN32)

private:
    const PluginManager* const m_pluginManager;
    const QString m_pluginHomeDir;
    const QString m_libName;
    QString m_originalDllDirectory;
};

//-------------------------------------------------------------------------------------------------

PluginLoadingContext::PluginLoadingContext(
    const PluginManager* pluginManager, QString pluginHomeDir, QString libName)
    :
    d(new PluginLoadingContextImpl(pluginManager, pluginHomeDir, libName))
{
}
