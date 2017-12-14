#include <iostream>
#include <sstream>
#include <stdexcept>
#include <set>

#include <boost/optional.hpp>

namespace nx {
namespace root_tool {

typedef const std::string& Argument;
typedef const boost::optional<std::string>& OptionalArgument;

/** Mounts NAS from url to directory for real UID and GID. */
int mount(Argument url, Argument directory, OptionalArgument username, OptionalArgument password);

/** Unounts NAS from directory. */
int unmount(Argument directory);

/** Changes path ownership to real UID and GID. */
int chengeOwner(Argument path);

/** Touches a file and gives ownership to real UID and GID. */
int touchFile(Argument filePath);

/** Creates directory and gives ownership to real UID and GID. */
int makeDirectory(Argument directoryPath);

/** Installs deb package to system. */
int install(Argument debPackage);

/** Prints real and effective UIDs and GIDs. */
int showIds();

/** Set real UID and GID to effective (some shell utils require that). */
void setupIds();

} // namespace root_tool
} // namespace nx
