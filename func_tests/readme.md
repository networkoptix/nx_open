# Functional Tests

## Setup

On Ubuntu/Debian `cd .../nx_vms/func_tests` and run `BASE_DIR=~/develop ./bootstrap.sh`. It will
install VirtualBox and prepare virtual environment in `~/develop/func_tets-venv`. Activate it:
`. ~/develop/func_tests-venv/bin/activate`.

Manual setup: https://networkoptix.atlassian.net/wiki/spaces/SD/pages/85717618.

Install PyCharm Community Edition: https://www.jetbrains.com/pycharm/download/#section=linux. With
it, you can do step-by-step debugging, run tests conveniently, and get as much IDE features (code
completion, quick checks, refactorings) as possible for Python.

## Writing a Test

Read `pytest` basics on https://docs.pytest.org/en/latest/getting-started.html. What's below is
specific to this project.

Start with empty test. Create `tests/test_example.py`; name should start with `test_` or end with
`_test`. Add test, which is just a function starting with `test_`:
```{.py}
def test_empty():
    pass
```

The test is already in the test suite and will be executed by CI if committed and pushed.

Run it:
```{.sh}
pytest tests/test_example.py
```

More on running selected tests:
https://docs.pytest.org/en/latest/usage.html#specifying-tests-selecting-tests

Use Mediaserver API:
```{.py}
def test_online(one_mediaserver_api):
    assert one_mediaserver_api.is_online()
```

What is passed into test function is a fixture. Fixtures is how the framework is used within tests.
More on this: https://docs.pytest.org/en/latest/fixture.html.

To run this test against local Mediaserver, do `export PYTHONPATH=.`. Otherwise, `pytest` can't
import `fixtures.local_mediaservers` plugin. Run it on you local mediaserver first:
```{.sh}
pytest -p fixtures.local_mediaservers tests/test_example.py::test_online
```

Let framework setup VM with mediaserver:
```{.sh}
pytest tests/test_example.py::test_online
```

## Artifacts

Use local directory and save artifacts, which are just files stored in specific folder:
```{.py}
def test_files(node_dir, artifacts_dir):
    path = node_dir / 'random_file'
    path.write_bytes(b'\xff\xfeh\x00e\x00l\x00l\x00o\x00')
    assert path.read_text(encoding='utf-16') == u'hello'
    copy_file(path, artifacts_dir / path.name)
```

In `pytest` terminology, "node" is parametrized test function, hence the fixture name.

Run:
```
pytest --work-dir=~/develop/func_tests-work tests/test_example.py::test_files
```

What is in `artifacts_dir` goes to JunkShop as artifact.

On CI, entire `work_dir` content are published in
http://artifacts.enk.me/develop/vms/3831/default/all/functests/ where `3831` is a build number.

Locally it looks this way:
```
(ins)(func_tests-venv) user@host:~/Development/nx_vms/func_tests$ tree ~/develop/func_tests-work/
/home/george/develop/func_tests-work/
├── latest -> /home/george/develop/func_tests-work/run_20180920_201942
└── run_20180920_201942
    ├── debug.log
    ├── info.log
    └── tests
        └── test_example.py
            └── test_files
                ├── artifacts
                │   └── random_file
                └── random_file
```

Only 5 last runs are saved.

## Publish to JunkShop (Optional)

It's possible to publish local test result to JunkShop.

1. Checkout devtools repo from ssh://hg@enk.me/devtools.
2. Add devtools/ci to PYTHONPATH:
`set environment variable PYTHONPATH=$DEVELOPMENT_DIR/devtools/ci/junk_shop.`
DEVELOPMENT_DIR is directory where you checked out devtools.
3. Enable Junk Shop plugin in pytest:
`PYTEST_PLUGINS=junk_shop.pytest_plugin`
or add parameter: `pytest -p junk_shop.pytest_plugin ...`.
More on plugins: https://docs.pytest.org/en/latest/plugins.html and `pytest --help`.
4. Add DB access run option:
`--capture-db=postgres:PASSWORD@10.0.0.113.`
Ask for password in person. 10.0.0.113 is current address of Postgres DB.
5. Add build run option:
`pytest ... --build-parameters=project=test,branch=vms,build_num=444,platform=linux-x64,customization=default,cloud_group=test`.
This is where results will appear in JunkShop.
This example has URL: http://junkshop.enk.me/project/test/vms/444/default/linux-x64.
Please, use project `test` which is for local runs and tests.
Don't publish your results to `ci` and `release` projects.

## Directory Structure

- func_tests — all functional tests code is located in nx_vms/func_tests:
  - fixtures — fixtures for tests, specific for pytest;
  - framework — this module is set of libraries for environment setup and mediaserver testing;
  - self_tests — (unit) tests of framework, generally quick and simple;
  - windows_scripts — useful scripts for Windows VMs setup;
  - bootstrap.sh — bootstrap script: sets up test environment.

## Framework Structure

Class Hierarchies:
- `Hypervisor` and `VirtualBox` — compatibility layer for hypervisors.
  This classes know nothing about VM naming and locks on VMs used for parallel run.
- `VMType`, `VMFactory`, `Registry` — allocating, reusing and creating VMs.
- `OSAccess` and ancestors — uniform access layer to Linux and Windows, operations that can be
  implemented (even if only theoretically) for Linux and Windows. `OSAccess` family knows nothing
  about Mediaserver. `OSAccess` uses classes:
  - `FileSystemPath` — pathlib-like interface for paths: not all operations are implemented,
    exceptions are uniform. Platform classes: `SSHPath`, `SMBPath`.
  - `Networking` — common place for networking-related operations.
    - `PosixShell` — POSIX-specific interface, subclasses: `LocalShell` and `SSH`.
    - `WinRM` — class with Windows-specific interface. It can execute CMD and PowerShell scripts
      remotely, make some WSMan operations (effectively, it's HTTP/SOAP OS API).
- `Installation` — responsible for Mediaserver installation.
  - `Service` — service in OS.
- `Mediaserver` — simple aggregate of API, installation and other classes with some methods.
  - `MediaserverApi` — simple class to talk to HTTP API.
