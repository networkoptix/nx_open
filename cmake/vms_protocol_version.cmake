# VMS Protocol version defines compatibility between different VMS parts such as the desktop client,
# the VMS server and the cloud. Usually it should be increased when some api structures are changed
# and logical or network-level compatibility is lost.

# In-version protocol number should be reset to 00 when updating the project version
# Important: when changing the protocol version, corresponding changes must be added to the cloud
# release branch. Ask #akolesnikov for the details.
# YOU MUST FIX THE COMMENT to make sure it will generate a merge conflict if somebody also updates
# the protocol version. Prefer to use issue number and title as a comment.
set(_vmsInVersionProtocolNumber 00) # Make protocol version depend on VMS project version
set(vmsProtocolVersion "${PROJECT_VERSION_MAJOR}${PROJECT_VERSION_MINOR}${_vmsInVersionProtocolNumber}")
unset(_vmsInVersionProtocolNumber)
