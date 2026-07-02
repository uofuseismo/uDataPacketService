#include <string>
#include "uDataPacketService/version.hpp"

using namespace UDataPacketService;

int Version::getMajor() noexcept
{
    return uDataPacketService_MAJOR;
}

int Version::getMinor() noexcept
{
    return uDataPacketService_MINOR;
}

int Version::getPatch() noexcept
{
    return uDataPacketService_PATCH;
}

//NOLINTBEGIN(bugprone-easily-swappable-parameters)
bool Version::isAtLeast(const int major, const int minor,
                        const int patch) noexcept
//NOLINTEND(bugprone-easily-swappable-parameters)
{
    if (uDataPacketService_MAJOR < major){return false;}
    if (uDataPacketService_MAJOR > major){return true;}
    if (uDataPacketService_MINOR < minor){return false;}
    if (uDataPacketService_MINOR > minor){return true;}
    if (uDataPacketService_PATCH < patch){return false;}
    return true;
}

std::string Version::getVersion() noexcept
{
    std::string version{uDataPacketService_VERSION};
    return version;
}

std::string Version::getTag() noexcept
{
    std::string tag{uDataPacketService_GITTAG};
    return tag;
}

std::string Version::getVersionWithTag() noexcept
{
    auto tag = Version::getTag();
    if (tag.empty())
    {
        return Version::getVersion();
    }
    else
    {
        return Version::getVersion() + "-" + tag;
    }
}

