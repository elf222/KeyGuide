// settings/app_paths.cpp
#include "settings/app_paths.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>
#if !defined(_WIN32)
#include <cerrno>
#include <unistd.h>
#endif

namespace
{
    std::filesystem::path EnvironmentPath(const char* name)
    {
        const char* value = std::getenv(name);
        return value && *value ? std::filesystem::path(value) : std::filesystem::path();
    }
} // namespace

std::filesystem::path HomeDirectory()
{
#if defined(_WIN32)
    auto home = EnvironmentPath("USERPROFILE");
#else
    auto home = EnvironmentPath("HOME");
#endif
    return home.empty() ? std::filesystem::current_path() : home;
}

std::filesystem::path AppDataDirectory()
{
#if defined(__APPLE__)
    auto directory = HomeDirectory() / "Library" / "Application Support" / "KeyGuide";
#elif defined(_WIN32)
    auto base = EnvironmentPath("APPDATA");
    auto directory = (base.empty() ? HomeDirectory() : base) / "KeyGuide";
#else
    auto base = EnvironmentPath("XDG_CONFIG_HOME");
    auto directory = (base.empty() ? HomeDirectory() / ".config" : base) / "keyguide";
#endif
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    return directory;
}

bool WriteFileAtomically(const std::filesystem::path& path, const std::string& contents,
                         bool owner_only)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
#if !defined(_WIN32)
    // mkstemp: unpredictable name, created exclusively (never an existing file or symlink) with
    // mode 0600, so every file KeyGuide writes is owner-only.
    (void)owner_only;
    std::string temporary = path.string() + ".XXXXXX";
    const int descriptor = mkstemp(temporary.data());
    if (descriptor < 0)
    {
        return false;
    }
    bool written = true;
    for (std::size_t offset = 0; written && offset < contents.size();)
    {
        const auto count = ::write(descriptor, contents.data() + offset, contents.size() - offset);
        if (count < 0 && errno == EINTR)
        {
            continue;
        }
        written = count > 0;
        offset += written ? static_cast<std::size_t>(count) : 0;
    }
    written = ::close(descriptor) == 0 && written;
#else
    auto temporary = path;
    temporary += ".tmp";
    bool written = true;
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            return false;
        }
        if (owner_only)
        {
            std::filesystem::permissions(temporary,
                                         std::filesystem::perms::owner_read |
                                             std::filesystem::perms::owner_write,
                                         std::filesystem::perm_options::replace, error);
            if (error)
            {
                file.close();
                std::filesystem::remove(temporary, error);
                return false;
            }
        }
        file << contents;
        written = static_cast<bool>(file.flush());
    }
#endif
    if (written)
    {
        std::filesystem::rename(temporary, path, error);
    }
    if (!written || error)
    {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

bool ReadWholeFile(const std::filesystem::path& path, std::string& contents)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    contents = buffer.str();
    return true;
}
