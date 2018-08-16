# Logging Filtering Proposal

## The problem

Logging of external commands or HTTP connections are quite verbose on
DEBUG and even on INFO level. Although, when called from specific places,
those logging could be meaningful. The task is control presence of
particular log entries in runtime.

## Proposed Solution

Let's introduce a context managed and/or decorator that can change level 
of some other particular logger:
```python
@logging_level('INFO', 'framework.ssh', 'framework.winrm')
@logging_level('DEBUG', 'framework.ssh.stderr')
def foo():
    winrm = WinRM()
    winrm.run_command()
```

Since all logger name are module names, `__module__` can be used:
```python
@logging_level('INFO', WinRM.__module__, SSH.__module__)
@logging_level('DEBUG', 'framework.ssh.stderr')
def foo():
    winrm = framework.winrm.WinRM()
    winrm.run_command()
```

Level can be bound to level of current logger:
```python
@logging_level('INFO', WinRM.__module__, SSH.__module__)
@logging_level(_logger.getEffectiveLevel(), 'framework.ssh.stderr')
def foo():
    winrm = framework.winrm.WinRM()
    winrm.run_command()
```

## In Other Words

Author of any function can basically say,
"if this function is debugged, verbose logging from other function
is worth to reveal too."
Or,
"In this test, logging of these modules
should be shown and saved."

## Variations

- With special treatment, decorator can be used as context manager.
- `__module__` attribute may be queried implicitly.
- `_logger.getEffectiveLevel()` may be called at runtime, not
when defining function. Mind that in this case level may get propagated
and must be used wisely.
- Similar decorator may be developed for the classes and modules,
but it must be used with great caution: classes must have predictable
and well-defined dependencies.

## Extensions

As next step, this system may get more fine-grained control:
- configuration:
  - level of propagation (0 – disabled at all);
  - enable or disable this for specific loggers;
- set level relative to level of current logger (if current logger
is at `DEBUG`, specified should be at least at level `INFO`).
