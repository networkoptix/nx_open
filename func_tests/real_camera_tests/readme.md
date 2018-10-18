# Real Camera Tests (RCT)

RCT is a pytest based functional test for testing how different versions of NX VMS Server handle 
certain cameras provided by user environment. In the end of its run detailed report is generated 
with detailed description of test results. 

Expected use case:

- Prepare camera network with cameras to test on.
- Describe cameras and tests in config, by default `expected_cameras.yaml`.
- Run RCT on stable VMS version, until you know how config and cameras work.
- Run RTC on VMS version you want to test, report differences will indicate new version problems.

NOTE:

Most of the real world cameras are buggy and work vary unstable. It is recommenced to run the same 
RCT on the same VMS version several times before making any conclusions. Some problems may be random 
and never reproduce any more. It is also recommended to reboot all cameras before each RCT run,
because at any moment any camera may start to fail on certain requests or even freeze until reboot.


## Requirements

NOTE: Only linux VMs are tested so far, it is recommended to use these.

Most of the requirements are the same as for every other functional test, so make sure you can run
these. On top of that you also need a designated network with cameras. To run the test use:
```bash
    pytest real_camera_tests/test_server.py \
        --rct-interface=$NETWORK_INTERFACE_WITH_CAMERAS \
        --rct-network=$NETWORK_IP/$NETWORK_MASK \
        --rct-expected-cameras=$CAMERAS_CONFIG_TO_OVERRIDE_DEFAULT
```

After successful or failed run test results will be generated:

- `module_information.json (yaml)` - api/moduleInformation of tested server.
- `all_cameras.json (yaml)` - ec2/getCamerasEx at the end of test run.
- `test_results.json (yaml)` - detailed test results with cameras and their stages.


## Test Scenario

### Prepare Single VM with Running Configured VMS Server

On top of then test:

- Plugs bridged interface with cameras into VM so server can see them.
- Creates and activated professional license for 100 cameras.


### Read Expected Cameras Config with their Test Stages

By default the config is  `expected_cameras.yaml`, format fallows:

```yaml
<camera>:
  <stage> [if <condition>]: <configuration>
  <other_stages>: ...
  ...

<other_cameras>: ...
...
```

where:

- `<camera>` - camera identifier in test results and logs, e.g. vendor and model.
- `<stage>`, `<configuration>` - describe test stages for this camera, complete list of stages 
  with descriptions can be found in `stages.py` comments.
- `<condition>` (optional) - allows to disable stage or override it's configuration for different
  VMS servers, e.g. `discovery if version > '3.1': ...` if camera was not supported prior 3.1.
  
Config may contain any number of cameras and stages per each camera, however there are 2 mandatory
stages:

- `discovery` - camera without this stage will be removed from config.
- `authorization` - cameras will remain in config but no stages will be executed.
  
  
### Camera Stages Execution

Test cases for all cameras are executed simultaneously, while stages for each camera are executed
one by one in order predefined by config. Each stage is running until it is succeed or interrupted
by stage timeout. Most of the stages are independent and will be executed regardless of results of
other stages, except "essential" stages ('discovery' and 'authorization'). When they fail all other
stages are skipped.

When the last stage of the last camera is finished the report is generated:

```yaml
<camera>:
  status: success / failure
  start_time: YYYY-MM-DD HH:mm:ss.zzzzzz+00:00
  duration: HH:mm:ss
  warnings: [...]  # Optional, omitted if empty.
  stages:
    <stage>:
      status: success / halt / failure  # Halt means timed out on retry.
      start_time: YYYY-MM-DD HH:mm:ss.zzzzzz+00:00
      duration: HH:mm:ss
      errors: [...]  # Optional, present only on failure.
      exception: [...]  # Optional, present only if stage was interrupted by exception.
      message: ... # Optional, present only on halt.
      
    <other_stages>: ...
    ...
    
<other_cameras>: ...
...
```

If server discovers some cameras, which are not specified in config, they will be added to report
with failures.


### Server Stages Execution

Config also supports server stages with some queries to server to execute and compare results:

```yaml
SERVER_STAGES:
  before: # Executed before camera stages.
    <query>: <expected_result>
    ...
    
  after: ... # Executed just after camera stages are finished.
  delayed: ... # Executed at the vary end with a delay.
```

At the end these stages provided the similar result to camera stages.


