# Portable application runtime

Phase 1D integrates the existing models without changing the BLE v1 or
recording v1 formats. Before integration, the circular and ride fixtures
ran different model instances, and the recorder accepted a reference to
mutable telemetry. The recorder also combined scheduling with storage I/O.
The application now owns the live models and schedules recording work. The
worker consumes owned samples and reuses the existing writer and adapters.

## Ownership and handoff

| Owner | Inputs and responsibilities | Outputs |
|---|---|---|
| Acquisition adapters | Poll telemetry at a provisional 1–2 Hz, subject to verified ECU capability. Receive navigation events as sent, approximately 1 Hz during active guidance. Own transport/link callbacks. | Value-owned readings and fixed-size navigation frames, tagged with the link generation returned by the reducer. |
| Application state owner | Apply serialized link, telemetry, navigation, clock and settings events. Update engine state, schedule work, and publish the latest snapshot. | Copyable application snapshots, owned recording work and queue counters. |
| Recording worker | Consume work in FIFO order, reserve a session, append, and request filesystem sync. Inspect retained files and pass recovery and writer status back as copied values. | Recording status, errors and recovery summary. |
| Settings worker or boot adapter | Load the small preferences record or defaults. Persist changes through the existing settings model. | Confirmed preferences and load/save result. |
| Display owner | Consume a copy of the latest application snapshot. The host renderer receives only the snapshot projection. | Circular display and diagnostic views. |

The portable classes create no threads and contain no locks. Only their
owner accesses mutable objects. The accessors and queue operations are
also owner-only. An embedded adapter must serialize reducer ingress,
copy values through its task channels, preserve FIFO storage results, and
discard work and callbacks from an earlier device boot. One recording
worker processes one work item at a time. Adapters must check that a
link-up/down callback belongs to their current physical connection before
issuing it to the reducer. The generation check rejects delayed samples
and packets after reconnect. Navigation still checks v1 session and sequence.

ECU disconnect invalidates RPM, oil, intake and ECU/mock voltage immediately.
Independent adapter or battery voltage can remain until its own freshness
expires. Phone loss clears guidance immediately. Snapshot publication
returns copies, so a caller retaining an earlier snapshot sees a stable
historical view. Display mode is part of the loaded or confirmed user
preferences. The simulator renders the SVG and diagnostics from a single
snapshot response and discards superseded responses.

## Scheduling and backpressure

The caller supplies a nondecreasing monotonic clock and calls the reducer
after acquisition batches, timers and significant events. Schedule wakeups
at recording/freshness deadlines or combine them with normal acquisition.
There is no internal polling. Source freshness remains a 3000 ms policy.
The reducer publishes on scheduled ticks at most once per 100 ms. Boot,
link and preference events can publish immediately. The host also forces
publication after a completed replay step. A renderer can skip intermediate
snapshots and consume the latest sequence.

Record at the current timestamp when at least one second has elapsed since
the previous attempt. Request sync after five seconds from boot or the last
request and on a fresh running-to-stopped transition. A stop can request an
extra sample between periodic samples. A large time jump produces at most
one current scheduled sample and sync request. Missed intervals are skipped.
A regressed event or tick is rejected and counted. A cached running RPM
that has already expired cannot turn a later zero into a confirmed stop.

The fixed FIFO holds four items in the desktop configuration. A full queue
drops the new sample and increments its drop count. Storage delay therefore
cannot force the state owner to wait. A sync request attaches to the last
accepted item currently in the queue, even if its accompanying stop sample
was dropped. The worker writes that item before syncing and then proceeds
to later items. If the queue is empty, a sync-only item can be queued.
Multiple syncs attached to the same tail coalesce using the most recent
request timestamp. The counters distinguish requests, coalescing, worker
sync attempts, storage errors, depth and high-water mark. Scheduling and
session time use capture time, independent of worker delay. The dequeued
item is owned by the worker and is outside the four-item backlog.

## Boot, recovery and media

1. Load and validate preferences outside the live reducer. Defaults permit
   early display if the settings worker is delayed or unavailable.
2. Create a new application instance with disconnected links, unavailable
   live readings, a fresh monotonic origin and pending archive status.
   Publish it immediately.
3. Inspect retained files on a storage worker and report a summary. Call
   the recorder for the first queued sample to reserve the new session and
   write its header. A missing store reports degradation as soon as the
   worker attempts an operation. Acquisition and rendering continue.
4. On abrupt cutoff, discard the volatile reducer, queue and recorder
   without writing an end marker. On the next boot, read earlier files
   without modifying or reopening them. Report interruption, truncation
   or corruption and the valid prefix. A new ID is reserved within the
   retained storage directory. Navigation and telemetry caches start empty.

Host archive inspection can stream one report per file and retain only
aggregate counts plus the most recent ID/outcome. It reads through the
valid prefix of each retained file. Reservation scans names for the
largest retained ID, then exclusively creates the next file. Both are
linear in the retained archive and stay off the state owner. The desktop
replay deterministically runs the worker between steps. An embedded
integration may cap or incrementally schedule archive inspection and
separate it from opening the current session if measured boot costs
require it. It may need a bounded archive policy or a validated index.
No timing measurements were collected for this phase.

The host assumes the same mounted directory while recording. It can reject
a vanished or shortened file. A data file with the same name and plausible
length on a replacement medium is not a reliable identity match. Live
replacement remains unsupported. A future mount adapter must stop the
writer when it detects a medium change and report degradation. Session IDs
are scoped to the retained directory and may repeat on a different or
reinitialized volume. A cross-media identifier requires a separate
namespace design.

## Clock and durability

Freshness, scheduling and elapsed time use only the supplied monotonic
clock. An optional absolute reading carries an explicit unknown, unverified
or trusted quality. When an anchor is supplied, the reducer advances it
using its monotonic delta. Missing anchors or arithmetic overflow clear
absolute time. Future RTC code must determine hardware clock quality.
The snapshot and recording can carry unknown time without blocking boot.

The host drains application buffers and requests file sync using POSIX
`fsync` or Windows `_commit`. Parent directory metadata and controller
caches are outside that guarantee. A recovered valid prefix is an observed
property of the available bytes and does not identify the exact cutoff.
The embedded driver, actual microSD and filesystem, power interface and RTC
must be measured separately. See the [recording specification](protocol/ride-recording.md)
and [Phase 1D verification record](verification-phase-1d.md).
