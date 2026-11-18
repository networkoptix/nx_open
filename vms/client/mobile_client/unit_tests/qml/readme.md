# Mobile client QML tests

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

[Qt Quick Test](https://doc.qt.io/qt-6/qtquicktest-index.html) tests, built into
`mobile_client_qml_ut` and run by CTest like any other `*_ut` target.

## Writing a test

For a test of a single control, `ControlTestCase` from `Nx.Test` can be used, see `tst_Banner.qml`.

Note: `appContext` in the unit tests mode, the client QML modules and the `image://skin/` image
provider are available.

Note: Settings are shared by all test files and some persist between runs, so a test restores the
ones it changes in `cleanup()`.

## Running

```
<build-dir>/bin/mobile_client_qml_ut
```

- `Nx.Mobile.Controls.Banner::test_shown` runs a single function.
- `-platform cocoa` (or `windows`, `xcb`) shows real windows instead of `offscreen`.

To run the tests and the client QML from the source tree without rebuilding, run from this
directory:

```
QML_IMPORT_PATH=../../static-resources/qml:static-resources/qml \
    <build-dir>/bin/mobile_client_qml_ut -input static-resources/tests
```
