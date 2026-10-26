# Validation and scope

## Frame cleanup

The observed failure was a null result from the frame registration lookup being passed to unlink and node-free operations. The repair skips those two operations only when the entry is absent. The enclosing frame destructor still releases its name and continues cleanup.

The offline test reproduces the original access violation, loads the actual MIX, and checks normal return, stack/callee-saved register preservation, outer destructor continuation, normal-node unlink/free, and neighboring data preservation.

## Post-game callbacks

The second observed crash occurred when dzclient requested player information after the game manager was gone. Returning a fabricated player value would still leave VM-dependent script execution active, so the repair handles the dispatcher lifetime instead.

On an observed active-to-absent transition it retires the old callback and the captured FIFO prefix. Completed steps and pending old work survive retries. Newly registered callbacks/events are preserved, and old events are not executed in the next map. The native UI tick continues.

Tests cover initial menu state, gameplay, teardown, new registration during the menu, next-map execution, and cleanup failure followed by both an absent-state retry and immediate next-map retry. Queue operations and script execution are modeled; this does not establish recovery from arbitrary native container corruption.

## Menu cursor

A custom map can override the `Cursor` skin entry with a map-only model. The native resolver prioritizes that override, including when the menu requests its cursor. The repair uses the native selected/default skin path for this one key only while the actual menu root is active and no game manager exists.

The native refresh path is followed by active-cursor binding. A loaded model and the bound sprite must both be observed before refresh is complete. Silent load failure retries on a later UI tick.

Tests exercise the original resolver, refresh and binding control flow using controlled skin tables, resource loading and final sprite assignment. They check menu restoration, default-category fallback, retry, other skin keys, loading-root isolation, and return to custom cursors in the next map.

## Evidence levels

- Actual play feedback confirmed that the two reported exit crashes stopped.
- Native offline checks passed for the published source and artifacts.
- Actual play feedback also confirmed that the menu cursor is visible after the repair.
- No automated check here launches the game, drives its UI, or establishes GPU output.

Independent source review was performed before publication. These fixes address the investigated paths; they are not a claim to resolve every Warcraft III exit crash.
