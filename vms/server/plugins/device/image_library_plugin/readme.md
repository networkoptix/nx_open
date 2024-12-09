# Image Library Plugin

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

This plugin provides access to a directory with image files using Camera Integration API (mainly,
by implementing `nxcip::DtsArchiveReader` interface).

Images from the specified directory are provided as a stream of frames with the specified duration
(in milliseconds). Images are read in unspecified order.

Specify a directory containing JPEG images using the URL in the form: `file://<absolute_path>`

This plugin is provided as an example of integration and is not intended for production purpose.
