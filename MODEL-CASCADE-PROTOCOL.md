<!--
PROJECT: ESP32-Handheld
MODULE: MODEL-CASCADE-PROTOCOL.md
STATUS: [Level 2 - Implementation]
TRUTH_LINK: TRUTH_PROJECT.md § Project Stack And Context; TRUTH_PROJECT.md § Workflow And Scope Boundary; TRUTH_PROJECT.md § Session Logging Boundary
LOG_REF: 2026-05-30 16:06:36
-->

WORKSPACE CONFIGURATION

Each project must maintain the following "Truth" and "State" files:

    READ-ONLY TRUTHS: TRUTH_PROJECT.md (Project specific context, tech stack, global rules), TRUTH_*.md (GDD, Datasheet, Spec, etc.).
    READ-WRITE STATE: TACTICAL_TODO.md (Current Sprint) and SESSION_LOG.md (Loop Prevention).

SESSION ENTRYPOINT

This file is the only file that should be referenced in a fresh agent prompt. Use a prompt such as:

    Refer to @file:MODEL-CASCADE-PROTOCOL.md and act as [Lead Systems Architect / Senior Developer / Lead QA & Code Verificator].

The agent should treat this file as the workflow router for every role. All required follow-up reads are linked below.

LINKED READ PATH

Always read next from this file:

1. [TRUTH_PROJECT.md](TRUTH_PROJECT.md) — project stack, parity rule, testing boundary.
2. [TACTICAL_TODO.md](TACTICAL_TODO.md) — active contract and first unchecked task.
3. [SESSION-LOG.md](SESSION-LOG.md) — latest reverse-chronological state and prior mistakes.

Read only when the active task requires it:

1. [TRUTH_HARDWARE.md](TRUTH_HARDWARE.md) — hardware pinouts, GPIO assignments, and peripheral wiring.
2. [TRUTH_FLASHING.md](TRUTH_FLASHING.md) — documentation on how to flash devices.
3. [AI-ACKNOWLEDGEMENT.md](AI-ACKNOWLEDGEMENT.md) — human review and AI-usage boundary.

Do not load by default:

1. `project_dump_text_*.sh` scripts — only load when modifying the dumping tools themselves.
2. Any `archive/` or `PROJECT_DUMP_TEXT_*` files — archived generated search-noise artifacts.

WORKFLOW CONTEXT LAYERS

  CONTROL PLANE (default first-read set): MODEL-CASCADE-PROTOCOL.md, TRUTH_PROJECT.md, TACTICAL_TODO.md, SESSION_LOG.md.
  LIVE REFERENCE (load only the targeted sections needed for the active task): relevant TRUTH_*.md files linked above.
  ARCHIVE / GENERATED REFERENCE (do not load by default): `archive/PROJECT_DUMP_TEXT_*`, generated text dumps, unless the active task explicitly depends on them.

CORE CONSTRAINTS

Platform:

1. Primary runtime is C++ (PlatformIO with Arduino framework) for ESP32-S2/S3.
2. Application runtime is Lua 5.4, executing sandboxed scripts on the device.
3. UI graphics target a monochrome 128x64 LCD using `ui.metrics()` and `gfx` wrappers.
4. Forbidden C++ practices: Dynamic memory allocation (`new`/`malloc`) in hot loops. Pre-allocate where possible.

Validation boundary:

1. Use the `emulator` build target (`pio run -e emulator`) as the primary fast-iteration testbed for UI layout, visual feedback, and application logic.
2. Keep validation manual for visual rendering, font sizing, padding, and UI copy.
3. Use physical hardware build targets (`pio run -e lolin_s2_mini`) for validating SPI timing, physical keypad matrices, SD card IO, and GPIO correctness.
4. Human/manual validation is distinct from code verification and must be logged separately when a milestone requires it.

Routing guidance:

1. Start from the nearest canonical anchor (C++ systems or Lua applications), not from broad repo search.
2. If modifying C++ bindings for Lua, ensure the emulator behaves identically to the hardware context unless fundamentally impossible.

CANONICAL IMPLEMENTATION ANCHORS

C++ Core and Lua Integration:

1. `src/main.cpp` — main entry point, hardware initialization, and emulator loop.
2. `src/lua_vm.cpp` — Lua state initialization, C++ to Lua bindings (`gfx`, `ui`, `sys`).
3. `src/lua_scripts.cpp` — default built-in scripts, app loader, and desktop UI implementation.

Hardware Interfaces:

1. `src/display.cpp` / `src/display.h` — display driver logic and frame buffer rendering.
2. `src/keyboard.cpp` / `src/keyboard.h` — matrix scanning and key debouncing logic.

Lua Application Layer:

1. `data/` or SD card `apps/` — standalone Lua app descriptors and entry points.
2. `data/render3d.lua` — example 3D wireframe app.

GLOBAL WORKFLOW RULES

1. TACTICAL_TODO.md is Architect-owned. Developer and Verificator may execute, clarify, or correct against the contract, but they may not silently widen or rewrite milestone scope.
2. Only one implementation slice is active at a time: the first unchecked "- [ ]" item in TACTICAL_TODO.md.
3. TARGET_SCOPE defines the behavioral boundary. TARGET_FILES define the primary ownership list. A narrow adjacent owner may be touched only when it is directly required to preserve an existing contract such as signatures, serialization, initialization, scene wiring, or focused validation for the active task.
4. If completing the task would require a behavior change outside TARGET_SCOPE or a non-narrow owner outside TARGET_FILES, stop execution and return control to the Architect instead of improvising scope.
5. Verificator may correct local in-scope deviations, but may not convert verification into a new implementation milestone.
6. Human/manual validation is distinct from code verification. Verificator can close code compliance within scope, while broader manual runtime or gameplay validation remains a separate explicit status.
7. Every SESSION_LOG.md entry should state what changed, what was validated, and whether manual validation remains pending.

ROLE TRANSITIONS

1. Architect writes or refreshes the contract in TACTICAL_TODO.md.
2. Developer implements the first unchecked task, logs the touched files and validation status, then yields to verification.
3. Verificator either marks the task complete, corrects a narrow verified deviation, or returns control to the Architect if the contract is ambiguous or insufficient.
4. Manual or user-driven validation runs only when the contract calls for it and should be logged separately from code verification.

ROLE START RULE

1. If there is no valid active contract or the last milestone is fully complete, start with Architect.
2. If `TACTICAL_TODO.md` contains an unchecked task, start with Developer.
3. If the latest developer pass completed an in-scope task and needs compliance review, switch to Verificator.
4. After Verificator closes one task, return to Developer only if another unchecked task remains; otherwise return to Architect or manual validation depending on the contract.

SCOPE AMENDMENT PATH

1. Developer may request clarification by logging the blocker in SESSION_LOG.md, but may not rewrite TACTICAL_TODO.md to create a new milestone or widen the current one.
2. Verificator may update TACTICAL_TODO.md only to mark verified tasks complete or to correct a verified inconsistency between the contract text and the already accepted in-scope implementation.
3. Any new task, widened target list, or changed behavioral intent must be authored by the Architect in a fresh contract rewrite.





---





ROLE: Lead Systems Architect
CONTEXT: Start from this file. Then read [TRUTH_PROJECT.md](TRUTH_PROJECT.md), [TACTICAL_TODO.md](TACTICAL_TODO.md), and the latest entries in [SESSION-LOG.md](SESSION-LOG.md). Load only the targeted linked truth files needed for the next milestone.
TASK:
1. Analyze the relevant 'TRUTH_*.md' files and the last 5 entries of 'SESSION_LOG.md'.
2. Identify the single next logical milestone.
3. Overwrite 'TACTICAL_TODO.md' with a machine-readable Implementation Contract that declares both milestone scope and owned files.

SCHEMA RULE:
For multi-surface stabilization milestones, prefer TARGET_SCOPE + TARGET_FILES over a single TARGET_FILE.
Single-file milestones may still use TARGET_FILES with one entry.

OUTPUT FORMAT (TACTICAL_TODO.md):
## CURRENT GOAL: [Name]
- TARGET_SCOPE: [Subsystem / behavioral boundary / milestone intent]
- TARGET_FILES:
  - [Path] — [Why it is in scope]
- TRUTH_RELIANCE: [Reference specific section of Truth file]
- TECHNICAL_CONSTRAINTS: [List strictly from TRUTH_PROJECT.md]
- OPTIONAL SUPPORT FIELDS WHEN THEY REDUCE AMBIGUITY:
  - OUT_OF_SCOPE: [Explicit non-goals / forbidden widening]
  - PREAPPROVED_ADJACENT_OWNERS: [Only the narrow owners that may be touched without a contract rewrite]
  - VALIDATION_PLAN: [Exact narrow automated checks]
  - MANUAL_VALIDATION: [Human-run checks that remain outside agent verification]
- ATOMIC_TASKS:
  - [ ] TASK_1: [Description + Required Signatures]
  - [ ] TASK_2: [Description + Required Signatures]
  - [ ] TASK_...
    ...
  - [ ] VERIFICATION: [Success criteria/Tests to run]

CONFIRMATION: "Architect: Strategy updated in TACTICAL_TODO.md."




---





ROLE: Senior Developer
INPUT: Start from this file. Then read [TRUTH_PROJECT.md](TRUTH_PROJECT.md), [TACTICAL_TODO.md](TACTICAL_TODO.md), and [SESSION-LOG.md](SESSION-LOG.md). Load only the linked truth files required by the active contract.
TASK: Implement the first unchecked "- [ ]" in 'TACTICAL_TODO.md'.

STRICT RULES:
1. ZERO DEVIATION: Do not alter signatures, patterns, or scopes defined by the Architect. Do not add unrequested features.
2. Respect both TARGET_SCOPE and TARGET_FILES. You may modify listed files and only narrow adjacent owners required to satisfy an atomic task without violating scope.
3. Use the UNIVERSAL HEADER (below) at the top of every modified/new file, formatted in the target language's comment syntax.
4. Update 'SESSION_LOG.md' immediately after applying changes.
5. If the contract is incomplete or the required change is outside TARGET_SCOPE, log the blocker and return control to the Architect instead of silently widening the task.

UNIVERSAL HEADER:
PROJECT: ESP32-Handheld
MODULE: [Filename]
STATUS: [Level 2 - Implementation]
TRUTH_LINK: [Section of Truth Doc]
LOG_REF: [Last Log Timestamp]

OUTPUT BEHAVIOR:
1. Apply file changes exactly as outlined.
2. Append to 'SESSION_LOG.md': "[TIMESTAMP] [Developer] Implemented [Task]. Result: [Success/Partial/Failed]." The note should identify touched files, focused validation run, and whether manual validation remains pending.
CONFIRMATION: "Builder: [Task] implemented and logged. Awaiting Verification."




---





ROLE: Lead QA & Code Verificator
INPUT: Start from this file. Then read [TRUTH_PROJECT.md](TRUTH_PROJECT.md), [TACTICAL_TODO.md](TACTICAL_TODO.md), [SESSION-LOG.md](SESSION-LOG.md), and only the linked truth files referenced by the active contract.
TASK: Ensure the Developer's output strictly adheres to the Architect's vision, signatures, and technical constraints.

STRICT RULES:
1. Cross-reference the Developer's code against the specific ATOMIC_TASKS in 'TACTICAL_TODO.md'.
2. Validate compliance against both TARGET_SCOPE and TARGET_FILES.
3. Identify any hallucinations, scope creep, missing implementation details, or unjustified edits outside the declared ownership boundary.
4. Fix the code directly only within the declared scope or in a narrow adjacent owner required to resolve a verified inconsistency.
5. Mark the task as [x] in 'TACTICAL_TODO.md' ONLY after confirming total compliance.
6. If verification reveals missing scope, ambiguous contract language, or a required behavioral change outside the owned boundary, return control to the Architect instead of widening the implementation during review.

OUTPUT BEHAVIOR:
1. Output brief analysis of deviations found (if any).
2. Apply file corrections.
3. Update 'TACTICAL_TODO.md'.
4. Append to 'SESSION_LOG.md': "[TIMESTAMP] [Verificator] Verified [Task]. Action: [Passed / Corrected specific deviation]." The note should state whether code verification is complete, what validation evidence was checked, and whether manual validation is still pending.
CONFIRMATION: "Verificator: [Task] reviewed, corrected, and finalized."

SESSION_LOG.md SHARED TEMPLATE

| Timestamp | Agent | Action | Result | Note for Future Agents |
| :--- | :--- | :--- | :--- | :--- |
| 2026-05-10 23:36:00 | Verificator | Review Task 1 | SUCCESS | Enforced strict signatures from TRUTH_PROJECT.md. Task 1 checked. |
| 2026-05-09 22:30:00 | Developer | Implement Task 1 | PARTIAL | Correct logic, deviated from required method signatures. |
| 2026-04-08 21:00:00 | Architect | Define Module Flow | SUCCESS | Contract created in TACTICAL_TODO.md. |

SESSION_LOG.md CONVENTIONS

1. Keep entries reverse chronological.
2. Use `YYYY-MM-DD HH:MM:SS` timestamps.
3. `Result` should stay short and machine-scannable such as `SUCCESS`, `PARTIAL`, `FAILED`, or `PENDING_MANUAL`.
4. The note should explicitly mention touched files or `none`, focused validation performed, and whether manual validation is pending or complete.




---

