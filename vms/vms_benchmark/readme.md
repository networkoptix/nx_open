# vms_benchmark command-line tool

This readme file is for developers. User readme is placed in the distribution directory:
`vms/distribution/standalone/vms_benchmark/files/readme.md`.

---------------------------------------------------------------------------------------------------
## Suggestions for future development

1. Use type annotations.

2. Refactor class Camera to use ServerApi instead of interacting via Server REST API by itself.

3. Develop Unit tests.

4. ServerApi: Develop generic request methods to unify authentication, parameters and result
processing, etc.

5. Support multiple customizations on single box.

6. INI overriding: make temp directory atomically.

7. Add automatic test finish detection. The test could be treated as finished when the stream lag
stops growing.

8. Get rid of using an external SSH tool for connection. It would be better to use an internal SSH
connection library with persistent connection support.

9. Split rtsp_perf and testcamera into library + binary pairs and develop bindings for libraries.
Use these bindings instead of spawning rtsp_perf and testcamera processes.

10. Split main.py into service objects.

11. Develop second connection type - telnet. It's needed for some box types (e.g. edge1).

12. Develop scripts for building `<platform>/vms_benchmark-dev/python`. The current version was made
by hand. Now this artifact is just a copied native Python with the required libs installed into it.

13. Write documentation: in-code comments for classes and other entities.

14. Enable UPX to reduce executable on Windows.
