## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# VMS Protocol version defines compatibility between different VMS parts such as the Desktop Client,
# the VMS Server and the Cloud. Usually it should be increased when some API structures are changed
# and logical or network-level compatibility is lost.

# In-version Protocol number should be reset to 00 when updating the VMS version.
# ATTENTION: When changing the Protocol version, the corresponding changes must be added to the
# Cloud release branch. Inform the Team Lead of the Cloud team about the changes.
# ATTENTION: You must change the comment below to make sure it will generate a merge conflict if
# somebody concurrently updates the Protocol version. Prefer to use the Jira Issue id and title as
# a comment.
set(_vmsInVersionProtocolNumber 03) #< VMS-41371: Add saasState to the module information
set(vmsProtocolVersion "${PROJECT_VERSION_MAJOR}${PROJECT_VERSION_MINOR}${_vmsInVersionProtocolNumber}")
unset(_vmsInVersionProtocolNumber)
