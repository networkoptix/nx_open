# vms_benchmark command-line tool

This readme file is for developers. User readme is placed in distribution directory:
`vms/distribution/standalone/vms_benchmark/files/readme.md`.

---------------------------------------------------------------------------------------------------
## Suggestions for future development

1. Use type annotations.

2. Refactor class Camera to use ServerApi instead of doing stuff by itself.

3. Develop Unit tests for each method.

4. ServerApi: Develop generic request methods to unify authentication, parameters and result
processing, and etc.

5. Support multiple customizations on single box.

6. INI overriding: make temp directory atomically.

7. Add automatic test finish detection. The test could be treated as finished when stream lag stops
growing.

8. Get rid of using external SSH tool for connection. It would be better to use internal SSH
connection library with persistent connection support.

9. Split rtsp_perf and testcamera into library + binary pairs and develop bindings for libaries. Use
these bindings instead of spawn rtsp_perf and testcamera processes.

10. Split main.py into service objects.

11. Develop second connection type - telnet. It's needed for some box types (e.g. edge1).

12. Move ini and config defaults from code into ini and config files and pack these files into the
Benchmark binary.

13. Develop scripts for building python artefacts (used for Benchmark building) instead of making them
by hand.

14. Write documentation (for something like Doxygen).