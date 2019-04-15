# API changes TODO list

This is a TODO list for now, because this functionality is currently in mediaserver but may be
disabled by `nx_network_rest.ini`.

```
allowUrlParametersForAnyMethod=false #< Not sure if it's required in some cases.
allowGetModifications=false #< MUST HAVE, this functionality introduces vulnerabilities!
```
## Methods that are deprecated in 4.1

Record should be made in `api_changelog.html`. Client & Web & CI teams should be notified.
Changes in client code and UT should be made.

Full list of methods may be found by grep allowGetModifications.

## Methods support that have to be removed in 4.1

This functionality should be removed from server, as it's already deprecated.

- `GET api/activateLicense?key=<K>` -> `POST api/activateLicense` content `{"licenseKey": "<K>"}`.
- TODO

