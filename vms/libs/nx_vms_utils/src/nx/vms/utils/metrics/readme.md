# nx::vms::utils::metrics

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

## General Structure

The entry point of the subsystem is SystemController. Generally structure looks like this.

```
SystemController
* ResourceControllerImpl<Resource>: ResourceController[]
  * ResourceProvider<Resource>        --(add/remove)-->   ResourceMonitor
    * ValueGroupProvider<Resource>[]  ----(monitor)--->   * ValueGroupMonitor[]
      * ValueProvider<Resource>[]     ----(monitor)--->     * ValueMonitor[]
  * Rules                             ---(setRules)--->     * ExtraValueMonitor: ValueMonitor[]
                                                   \-->     * AlarmMonitor[]
  ----------|----------------------                       --------|------------------
            | --(manifest)--> API                                 | --(values)--> API
                                                                  | --(alarms)--> API
```

ResourceController is supposed to be implemented by inheriting ResourceControllerImpl<Resource>
where Resource as any user data (server, camera, etc). Generally it has to discover resources and
provide ValueGroupProvider<Resource>s with ValueProvider<Resource>s.
