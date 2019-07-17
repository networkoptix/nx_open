# nx::vms::utils::metrics

## General Structure

The entry point of the subsystem is Controller. Generally structure looks like this.

```
Controller
- AbstractResourceProvider[]     --(add/remove)-->  typename ResourceType[]
  - AbstractParameterProvider[]  ----(create)---->  - AbstractParameterMonitor[]
    _____  _____                              _____  _____
         \/                                        \/
      manifest()                                 values()

```

AbstractResourceProvider is supposed to be implemented by ResourceProvider<ResourceType> for each
type of resources (servers, cameras, etc). Generally it has to discover resources and provide
ResourceParameterProviders<ResourceType> which has generic implementations:
- SingleParameterProvider<ResourceType>:
  - without watch produces RuntimeParameterMonitor which provides current() value only,
  - with watch produces DataBaseParameterMonitor which saves values to DataBase and provides
    current() values as well as timeline() value changes;
- ParameterGroupProvider<ResourceType> (groups AbstractParameterProvider<ResourceType>[]):
  - produces ParameterGroupMonitor which aggregates children AbstractParameterMonitor).


## Rules

Controller has a set of rules which may:
- set a parameter status according to current values;
- calculate virtual parameters (based on values from DataBase).

```
Controller
- ResourceProvider[]
  |-> manifest -> Rules            -> manifest (with calculated parameters)
  |-> values   -> Rules & DataBase -> values (with statuses and calculated values)
```
