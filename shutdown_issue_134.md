# Shutdown issue: output teardown assertion and exit 134

As of: 2026-10-24. Status: reproduced, corrected in the working tree, and covered
by automated regression tests. Native Debian Sid debug and runtime launcher
rechecks confirm compositor exit code 0.

## Observed failure

The maintainer reported a shutdown crash in both debug and runtime launcher tests
1.8 and 1.9. Input and layout switching worked during the sessions.
Two debug-launcher log excerpts from native Debian Sid runs show the same failure:

```text
morph: types/output/output.c:404: wlr_output_finish: Assertion `wl_list_empty(&output->events.destroy.listener_list)' failed.
morph fatal signal 6
Aborted (core dumped)
Compositor crashed or exited with an error. Exit code: 134.
```

The recorded debug runs ended at 16:12:31 and 16:22:11 on 2026-10-24, with Morph
PIDs 5433 and 6457. Their log directory was
`/tmp/morph-xkb-report.o5SpWP/dbg-launcher-logs` on the test machine.
The runtime occurrence was reported by the maintainer; the supplied excerpts
both identify the debug launcher.

Exit 134 together with `SIGABRT` and this assertion is a compositor failure,
including when it happens only after the user requests quit. Client or satellite
`Broken pipe` messages do not make the compositor exit successful.

## Independent reproduction

The unchanged debug compositor was started with two headless outputs, the pixman
renderer, and `MORPH_X11=0`. No client applications or satellite were running.
GDB returned from `wl_display_run()` to exercise the normal cleanup path.
The same assertion was reached through:

```text
main
  wl_display_destroy
    wl_event_loop_destroy
      multi_backend_destroy
        backend_destroy
          wlr_output_destroy
            wlr_output_finish
```

Inspecting the remaining destroy listener identified `head_handle_output_destroy`
from wlroots output management. This reproduced the failure independently of
physical input, XKB layout changes, and satellite activity.

The automated regression added during the investigation also failed with the
original code. Under Meson's heap perturbation, it could fail with `SIGSEGV`
instead of the assertion, consistent with access to already-freed objects.

## Cause and correction

Backend output destruction was deferred until event-loop destruction, after
display-owned objects had already been destroyed. Output callbacks still used
the compositor's output-layout and output-manager pointers at that point.
In addition, `output_mgmt_fini()` detached its listeners but retained the manager
pointer, and `output_destroy()` rebuilt the configuration while the dying output
was still present in `server->outputs`.

The correction changes the lifecycle in three places:

- [`server_finish()`](src/main.c#L7358) destroys the backend while output layout,
  scene, protocol globals, and compositor configuration are still available.
- [`output_mgmt_fini()`](src/output_mgmt.c#L136) clears the compositor's manager
  pointer after detaching its listeners, preventing later configuration updates
  from reusing it. The display retains ownership of the manager object.
- [`output_destroy()`](src/main.c#L1775) removes the output from the compositor
  list before remapping input devices or rebuilding the output configuration.

The assertions remain enabled. The fix changes ownership and destruction order
rather than suppressing the failure.

## Automated validation

[`tests/test_output_shutdown.c`](tests/test_output_shutdown.c) links the actual
compositor object files, including `main()`, rather than duplicating its cleanup.
Test-only linker wrappers skip socket binding, retain the actual backend for the
hotplug check, and schedule normal event-loop termination with a timer.

| Test | Coverage | Runtime | Debug |
| --- | --- | --- | --- |
| `output-shutdown` | Full shutdown with two headless outputs | Passed | Passed |
| `output-hotplug` | Add/remove another output while running, then shut down | Passed | Passed |

```bash
meson test -C build output-shutdown output-hotplug --print-errorlogs
meson test -C build_dbg output-shutdown output-hotplug --print-errorlogs
```

The existing config, keyboard, and runtime shell tests also pass. The explicit
debug-launcher shell test passes. These launcher shell tests use a stub compositor;
they do not replace testing a real session.

ASan/UBSan checks for the new output tests pass with leak detection disabled.
A full LeakSanitizer run still reports retained scene, renderer, allocator, and
cursor resources. Those resource leaks remain an additional cleanup task; a
successful memory-access check must not be presented as a passing leak check.

Validation used wlroots 0.20.2, Wayland server 1.24.0, and libxkbcommon 1.8.0.
All standard test targets live in `build/` or `build_dbg/`.

## Manual validation results

The maintainer repeated both launcher checks natively on Debian Sid with the
corrected working-tree sources. The supplied excerpts record:

| Test | Time on 2026-10-24 | Compositor result |
| --- | --- | --- |
| 1.8, debug launcher | 18:34:22 | `Compositor exited normally with exit code 0.` |
| 1.9, runtime launcher | 18:38:11 | `Compositor exited normally with exit code 0.` |

Both launchers also report clean compositor exit and reliance on compositor-managed
shutdown hooks. No repeat of the output assertion or Morph fatal-signal marker
appears in the supplied extracts. This confirms that the reported exit-134 failure
did not recur in these two launcher rechecks. See [report_current.md](report_current.md)
for the result entries and original log paths. A separate rerun of test 1.7 was
not reported.

The excerpts still contain `Error: ExitFailure(1)` and, for runtime, `Broken pipe`.
Their exact emitting processes cannot be identified from these extracts. They
must be distinguished from the explicit compositor exit code 0. These results do
not establish that every companion process exits cleanly or that no companion
process creates a dump. Keep original dumps with their matching binaries.

## Related findings

- GEN-04 concerns missing `done` events in the ext-workspace protocol. It remains
  a separate protocol-consistency issue and is not resolved by this correction.
- [shutdown_issue_139.md](shutdown_issue_139.md) records an older exit-139
  investigation with satellite enabled. Its original root cause is unconfirmed.
  The current fix does not establish that the historical failure is resolved.
- [Output Shutdown Regression](docs/TESTS.md#output-shutdown-regression) documents
  test commands and limits; [Dev FAQ](docs/DEVFAQ.md#13-morph-exits-with-code-134-when-the-session-closes)
  explains how to interpret the exit status.
