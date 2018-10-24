# Real Camera Tests (RCT)

RCT is a pytest-based functional test for testing how different versions of NX VMS Server handle 
certain cameras provided by the user environment. In the end of each test run, the report is 
generated with a detailed description of the results. 

Expected use case:

- Prepare the camera network with the cameras to test on.
- Describe cameras and tests in the config, by default `expected_cameras.yaml`.
- Run RCT on a stable VMS version, until you know how config and cameras work.
- Run RTC on the VMS version you want to test, the differences in reports will indicate a problem in
  the new VMS version.

NOTE:

Most of the real world cameras are buggy and work very unstably. It is recommended to run the same 
RCT on the same VMS version several times before making any conclusions. Some problems may be random 
and never be reproduced. It is also recommended to reboot all cameras before each RCT run, because 
at any moment any camera may start to fail on certain requests or even freeze until reboot.


## Requirements

NOTE: Only Linux VMs are tested so far, it is recommended to use them.

Most of the requirements are the same as for every other functional test, so make sure you can run
them. Besides, you need a designated network with cameras. To run the test, use:

```bash
    pytest real_camera_tests/test_server.py \
        --rct-interface=$NETWORK_INTERFACE_WITH_CAMERAS \
        --rct-network=$NETWORK_IP_AND_MASK \
        --rct-expected-cameras=$CAMERAS_CONFIG_PATH
        
    # For example:
    NETWORK_INTERFACE_WITH_CAMERAS=eth1
    NETWORK_IP_AND_MASK=192.168.0.55/24
    CAMERAS_CONFIG_PATH=muskov_expected_cameras.yaml
```

After a successful or failed run, test results will be generated:

- `module_information.json (yaml)` - api/moduleInformation of tested server.
- `all_cameras.json (yaml)` - ec2/getCamerasEx at the end of test run.
- `test_results.json (yaml)` - detailed test results with cameras and their stages.


## Test Scenario

### Prepare Single VM with Running Configured VMS Server

On top of VM with VMS Server:

- The test plugs bridged interface with cameras into VM, so the server can see them.
- The test creates and activates a professional license for 100 cameras.


### Read Expected Cameras Config with their Test Stages

By default the config is `expected_cameras.yaml`, the format follows:

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
- `<stage>`, `<configuration>` - describes test stages for this camera; the complete list of stages 
  with descriptions can be found in `stages.py` comments.
- `<condition>` (optional) - allows to disable stage or override it's configuration for different
  VMS servers, e.g. `discovery if version > '3.1': ...` if the camera was not supported prior to 
  VMS 3.1.
  
The config may contain any number of cameras and stages per each camera, however there are 2
mandatory stages:

- `discovery` - camera without this stage will be removed from the config.
- `authorization` - cameras will remain in the config but no stages will be executed.
  
  
### Camera Stages Execution

Test cases for all cameras are executed simultaneously, while stages for each camera are executed
one by one in the order predefined by the config. Each stage is running until it succeeds or is 
interrupted by the stage timeout. Most of the stages are independent and will be executed regardless 
of the results of other stages, except for the "essential" stages ('discovery' and 'authorization'). 
When any "essential" stage fail, all other stages (including "essential" ones) are skipped.

When the last stage of the last camera is finished, json and yaml versions of the report are
generated:

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
      exception: [...]  # Optional, present only if the stage was interrupted by exception.
      message: ... # Optional, present only on halt.
      
    <other_stages>: ...
    ...
    
<other_cameras>: ...
...
```

If the server discovers some cameras which are not specified in the config, they will be added to 
the report with failures.


### Server Stages Execution

The config also supports server stages with some queries to the server to execute and compare the
results:

```yaml
SERVER_STAGES:
  before: # Executed before camera stages.
    <query>: <expected_result>
    ...
    
  after: ... # Executed just after camera stages are finished.
  delayed: ... # Executed at the very end with a delay.
```

At the end, these stages provide a similar result to the camera stages.


