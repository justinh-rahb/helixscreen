# HelixScreen Refactoring Plan

> **Document Purpose**: Reference guide for refactoring work and progress tracking
> **Created**: 2026-01-08
> **Last Updated**: 2026-01-09
> **Version**: 1.2 (Observer Factory Phase 4 complete - all target files migrated)

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Methodology](#methodology)
3. [Test-First Refactoring Protocol](#test-first-refactoring-protocol)
4. [Work Process & Agent Delegation](#work-process--agent-delegation)
5. [Tier 1: High-Priority Refactoring](#tier-1-high-priority-refactoring)
6. [Tier 2: Medium-Priority Refactoring](#tier-2-medium-priority-refactoring)
7. [Quick Wins](#quick-wins)
8. [Implementation Phases](#implementation-phases)
9. [Progress Tracking](#progress-tracking)
10. [Appendix: Code Examples](#appendix-code-examples)

---

## Executive Summary

### Codebase Snapshot
- **Headers**: 269 files
- **Source files**: 233 files
- **Largest files**: PrinterState (2808 lines), PrintStatusPanel (3782 lines), MoonrakerAPI (1190 lines)
- **State**: Mid-refactoring with good foundational patterns but incomplete adoption

### Key Findings

| Category | Finding | Impact |
|----------|---------|--------|
| **God Classes** | PrinterState (98 subjects), PrintStatusPanel (2925 lines) | High coupling, hard to test |
| **DRY Violations** | Observer boilerplate (129×), subject init (27×), modal cleanup (74×) | ~6500 lines duplicated |
| **Overlapping Systems** | 5 state management patterns, 3 async patterns, 3 error patterns | Cognitive load, inconsistency |
| **Incomplete Migrations** | SubjectManagedPanel exists but <50% adopted | Inconsistency tax |

### Estimated Total Impact
- **Lines to eliminate/consolidate**: ~8,000-10,000
- **Files affected**: ~100
- **Timeline (if prioritized)**: 4-6 weeks

---

## Methodology

### Analysis Approach
Three parallel investigations examined:
1. **DRY violations**: Repeated code patterns, duplicated logic, redundant utilities
2. **Structural complexity**: Large files, god classes, mixed concerns
3. **Architectural patterns**: Overlapping systems, inconsistent patterns, coupling

### Prioritization Criteria
Each item scored on:
- **Impact** (1-5): How much does this improve maintainability?
- **Risk** (1-5): How likely is this to introduce bugs?
- **Effort** (S/M/L): How long will this take?
- **Dependencies**: What must be done first?

---

## Test-First Refactoring Protocol

> **CRITICAL**: All refactoring work MUST follow test-first principles. Code changes without test coverage are not permitted.

### Why Test-First for Refactoring?

Refactoring by definition should not change external behavior. Tests are the **only way to verify** you haven't broken something. The larger the refactoring, the more critical the tests become.

```
┌─────────────────────────────────────────────────────────────────┐
│  REFACTORING WORKFLOW                                           │
│                                                                 │
│  1. CHARACTERIZE  →  2. REFACTOR  →  3. VERIFY  →  4. EXTEND   │
│     Write tests       Change code     Tests pass     Add new    │
│     for existing      structure       (same behavior) tests     │
│     behavior                                                    │
└─────────────────────────────────────────────────────────────────┘
```

### Phase 1: Characterization Tests (BEFORE any code changes)

**Purpose**: Capture existing behavior as executable specifications

**What to test:**
1. **Public API surface** - Every public method's expected behavior
2. **Edge cases** - Null inputs, empty states, boundary conditions
3. **Integration points** - How the component interacts with others
4. **Error paths** - What happens when things go wrong

**Characterization test pattern:**
```cpp
// Characterization test: captures existing behavior
// If this test fails after refactoring, you've changed behavior
TEST_CASE("PrinterState: extruder temp updates notify observers", "[characterization]") {
    PrinterState state;
    int notified_value = -1;

    // Capture existing behavior
    auto guard = state.observe_extruder_temp([&](int temp) {
        notified_value = temp;
    });

    state.set_extruder_temp(200);

    // This documents what the code CURRENTLY does
    // not what we think it should do
    REQUIRE(notified_value == 200);
}
```

**Checklist before refactoring:**
- [ ] All public methods have at least one test
- [ ] Edge cases are covered (null, empty, max values)
- [ ] Observer notification behavior is tested
- [ ] Thread-safety assumptions are tested (where applicable)
- [ ] All existing tests pass
- [ ] Test coverage report generated and saved

### Phase 2: Refactoring (with tests as safety net)

**Rules:**
1. **Run tests after every small change** - Not just at the end
2. **Commit frequently** - Every green test run is a potential save point
3. **One refactoring at a time** - Don't combine multiple changes
4. **If tests fail, revert** - Don't debug while refactoring

**Refactoring micro-steps:**
```
Change → Test → Green? → Commit → Next change
              ↓
            Red? → Revert → Investigate → Smaller change
```

### Phase 3: Verification

**After refactoring completes:**
- [ ] All characterization tests pass (behavior preserved)
- [ ] All existing unit tests pass
- [ ] All existing integration tests pass
- [ ] Manual smoke test of affected functionality
- [ ] Code review focuses on: "Does this change behavior?"

### Phase 4: New Tests for New Structure

**After refactoring is verified:**
- Add tests specific to the new structure
- These tests may overlap with characterization tests
- Characterization tests can be retired once new tests are comprehensive
- Add tests for any new capabilities enabled by the refactoring

### Test Categories

| Category | Purpose | When to Write | When to Run |
|----------|---------|---------------|-------------|
| **Characterization** | Capture existing behavior | BEFORE refactoring | During & after refactoring |
| **Unit** | Test isolated components | After structure exists | Always |
| **Integration** | Test component interactions | After integration points change | Before merge |
| **Regression** | Prevent specific bugs | After bug is fixed | Always |

### Coverage Requirements

| Refactoring Type | Minimum Coverage | Target Coverage |
|------------------|------------------|-----------------|
| God class decomposition | 80% of public API | 95% |
| Pattern extraction (observer factory) | 90% of pattern variations | 100% |
| API split | 85% of methods | 95% |
| Quick wins (utilities) | 100% of new code | 100% |

### Test File Organization

```
tests/
├── unit/
│   ├── test_printer_state.cpp           # Existing tests
│   ├── test_printer_temperature_state.cpp  # New domain state tests
│   └── ...
├── characterization/
│   ├── test_printer_state_char.cpp      # Characterization tests (temporary)
│   └── ...
└── integration/
    ├── test_ui_panel_integration.cpp
    └── ...
```

### Example: Test-First for PrinterState Decomposition

```cpp
// Step 1: Write characterization tests BEFORE any changes
// tests/characterization/test_printer_state_char.cpp

TEST_CASE("CHAR: PrinterState temperature subject notifications", "[characterization]") {
    PrinterState& state = PrinterState::get_printer_state();

    SECTION("extruder temp set triggers subject update") {
        int observed = -1;
        auto obs = ObserverGuard(state.get_extruder_temp_subject(),
            [&](lv_observer_t*, lv_subject_t* s) {
                observed = lv_subject_get_int(s);
            }, nullptr);

        state.set_extruder_temp(250);
        // Process async queue
        process_pending_async_calls();

        REQUIRE(observed == 250);
    }

    SECTION("bed temp set triggers subject update") {
        // Similar test for bed temp
    }

    // Test EVERY public method that will be affected by the decomposition
}

// Step 2: ONLY after all characterization tests pass and are committed,
// begin the refactoring work

// Step 3: After refactoring, these same tests verify behavior is preserved
// They should pass WITHOUT MODIFICATION

// Step 4: Add new tests for the new structure
// tests/unit/test_printer_temperature_state.cpp

TEST_CASE("PrinterTemperatureState: domain-specific tests", "[unit]") {
    PrinterTemperatureState temp_state;

    SECTION("initializes with default temps") {
        REQUIRE(temp_state.get_extruder_current() == 0);
        REQUIRE(temp_state.get_bed_current() == 0);
    }

    // Tests specific to the new decomposed structure
}
```

### Red Flags: Stop and Add Tests If...

- You're not sure what the code currently does → **Write characterization test**
- You found a bug while refactoring → **Write regression test, fix separately**
- Tests are flaky → **Fix flaky tests before continuing**
- You need to change test assertions → **You've changed behavior, investigate**
- Coverage dropped → **Add missing tests before continuing**

---

## Work Process & Agent Delegation

> **CRITICAL**: Refactoring work should leverage Claude Code agents for implementation while maintaining human oversight through structured code reviews.

### Agent Delegation Strategy

All substantial implementation work should be delegated to specialized agents. The main conversation handles **orchestration and review**, not direct implementation.

```
┌─────────────────────────────────────────────────────────────────────────┐
│  WORK DISTRIBUTION                                                      │
│                                                                         │
│  Main Session (Orchestrator)          Agents (Workers)                  │
│  ─────────────────────────────       ──────────────────                 │
│  • Plan approval                     • Write characterization tests     │
│  • Code review                       • Implement refactoring            │
│  • Decision making                   • Search/explore codebase          │
│  • Progress tracking                 • Run tests and report results     │
│  • Quality gates                     • Generate documentation           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Agent Types for Refactoring Work

| Task | Agent Type | When to Use |
|------|------------|-------------|
| Explore existing code structure | `Explore` | Before any refactoring to understand scope |
| Write characterization tests | `general-purpose` | Phase 1 of each refactoring item |
| Implement structural changes | `general-purpose` | Phase 2: actual code changes |
| Search for pattern occurrences | `Explore` | Finding all instances of DRY violations |
| Run tests and analyze failures | `Bash` | Continuous verification |
| Generate migration guides | `general-purpose` | Documentation for large changes |

### Work Chunk Definition

A "work chunk" is a **reviewable unit** of refactoring. Each chunk should:
- Be completable in 1-4 hours
- Have clear success criteria
- Be independently testable
- Be revertable if problems are found

**Chunk Size Guidelines:**

| Refactoring Type | Suggested Chunk Size |
|------------------|---------------------|
| God class decomposition | 1 domain state class extraction |
| Pattern factory | Factory implementation + 3-5 pilot migrations |
| Panel decomposition | 1 overlay/modal extraction |
| API split | 1 domain API extraction |
| Quick wins | Complete item |

### Code Review Requirements

> **RULE**: No refactoring merges without code review. Period.

#### Review Checkpoints

```
┌──────────────────────────────────────────────────────────────────────────┐
│  CHECKPOINT 1: After Characterization Tests                              │
│  ────────────────────────────────────────────                            │
│  Review Focus:                                                           │
│  • Do tests cover all affected public APIs?                              │
│  • Are edge cases included?                                              │
│  • Is thread-safety tested where applicable?                             │
│  • Do tests document current behavior accurately?                        │
│                                                                          │
│  Reviewer Action: ✅ Approve tests before refactoring begins             │
├──────────────────────────────────────────────────────────────────────────┤
│  CHECKPOINT 2: After Structural Changes                                  │
│  ────────────────────────────────────────                                │
│  Review Focus:                                                           │
│  • Does new structure match the plan?                                    │
│  • Are naming conventions followed?                                      │
│  • Is the code simpler than before? (or at least not worse)              │
│  • Are there any behavior changes? (should be NONE)                      │
│                                                                          │
│  Reviewer Action: ✅ Approve structure before migration                  │
├──────────────────────────────────────────────────────────────────────────┤
│  CHECKPOINT 3: After Migration Complete                                  │
│  ──────────────────────────────────────                                  │
│  Review Focus:                                                           │
│  • All characterization tests still pass?                                │
│  • No regression in test coverage?                                       │
│  • Backwards compatibility maintained?                                   │
│  • Documentation updated?                                                │
│                                                                          │
│  Reviewer Action: ✅ Approve for merge                                   │
└──────────────────────────────────────────────────────────────────────────┘
```

#### Review Types

| Review Type | Reviewer | When | Focus |
|-------------|----------|------|-------|
| **Self-review** | Author | Before any commit | Basic quality, tests pass |
| **Agent review** | Sonnet agent | After each chunk | Structure, patterns, consistency |
| **Human review** | Team member | Before merge to main | Behavior preservation, architecture |

#### Agent-Assisted Code Review

Use Claude Code agents for preliminary review before human review:

```bash
# Example: Request structural review from agent
# (In Claude Code conversation)

"Review the changes in src/printer/ for the PrinterTemperatureState extraction.
Focus on:
1. Does the new class follow existing patterns?
2. Are all subjects properly initialized and deinitialized?
3. Is thread-safety preserved?
4. Are there any behavior changes from the original code?"
```

**Agent review checklist:**
- [ ] Run Sonnet agent for structural review (not Haiku - per [L043])
- [ ] Agent identifies any concerns
- [ ] Concerns addressed before human review
- [ ] Human review focuses on higher-level concerns

### Work Session Structure

Each refactoring session should follow this structure:

```
SESSION START
│
├─► 1. Review plan and identify today's chunk
│      • Read REFACTOR_PLAN.md
│      • Identify specific deliverable
│      • Estimate time needed
│
├─► 2. Delegate characterization tests to agent
│      • Agent writes tests for affected code
│      • Review tests for completeness
│      • ✅ CHECKPOINT 1: Approve tests
│
├─► 3. Delegate implementation to agent
│      • Agent implements structural changes
│      • Tests run continuously
│      • Agent reports any issues
│
├─► 4. Review and iterate
│      • ✅ CHECKPOINT 2: Review structure
│      • Request changes if needed
│      • Agent makes adjustments
│
├─► 5. Verify and document
│      • All tests pass
│      • Update REFACTOR_PLAN.md progress
│      • ✅ CHECKPOINT 3: Final review
│
└─► SESSION END: Commit with conventional format
```

### Communication Protocol

**Agents should report:**
1. What they're about to do
2. What they did
3. Any concerns or blockers
4. Test results

**Main session should:**
1. Approve/reject agent proposals
2. Make architectural decisions
3. Resolve blockers
4. Track progress in this document

### Context Preservation & Critical Thinking

> **KEY PRINCIPLE**: The main session is the "brain" - agents are the "hands". Context and critical thinking live in the main session.

```
┌─────────────────────────────────────────────────────────────────────────┐
│  CONTEXT HIERARCHY                                                      │
│                                                                         │
│  Main Session (Preserved Context)                                       │
│  ────────────────────────────────                                       │
│  • Full project understanding        ← Accumulated across sessions      │
│  • Cross-cutting concerns            ← Sees how pieces connect          │
│  • Historical decisions              ← Why we chose this approach       │
│  • Failure patterns                  ← What didn't work before          │
│  • Architecture vision               ← Where we're heading              │
│                                                                         │
│  Agent Sessions (Fresh Context)                                         │
│  ──────────────────────────────                                         │
│  • Task-specific focus               ← Deep dive on one thing           │
│  • Implementation details            ← The "how" of execution           │
│  • Local discoveries                 ← Findings in specific files       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

#### Why Main Session Context Matters

1. **Cross-cutting concerns**: An agent refactoring PrinterState doesn't know about the parallel work on MoonrakerAPI. The main session coordinates.

2. **Pattern consistency**: The main session remembers "we decided to use `helix::` namespace" - agents might not.

3. **Risk awareness**: The main session knows "that area caused bugs before" - agents start fresh.

4. **Strategic decisions**: "Should we do X or Y?" requires project-wide context that agents lack.

#### Critical Thinking on Agent Output

**Never blindly accept agent output.** The main session must:

```
Agent Output Received
        │
        ▼
┌───────────────────┐
│ Does this align   │──No──► Request correction or
│ with the plan?    │        override with guidance
└───────────────────┘
        │Yes
        ▼
┌───────────────────┐
│ Does this affect  │──Yes─► Check for conflicts,
│ other areas?      │        coordinate if needed
└───────────────────┘
        │No
        ▼
┌───────────────────┐
│ Does this match   │──No──► Request adjustment to
│ existing patterns?│        match project conventions
└───────────────────┘
        │Yes
        ▼
┌───────────────────┐
│ Any concerns from │──Yes─► Address before proceeding
│ agent's report?   │
└───────────────────┘
        │No
        ▼
    Accept & Proceed
```

#### Information to Synthesize from Agent Output

When an agent completes work, the main session should extract and preserve:

| Extract | Purpose | Example |
|---------|---------|---------|
| **Discoveries** | Update mental model | "PrinterState has 15 more subjects than expected" |
| **Blockers** | Track for later resolution | "Can't test without mocking MoonrakerClient" |
| **Patterns found** | Apply elsewhere | "This panel uses the old observer pattern" |
| **Concerns raised** | Risk register | "Thread safety unclear in set_temp_internal" |
| **Files modified** | Change tracking | "Modified 3 files in src/printer/" |

#### Main Session Responsibilities

1. **Maintain the big picture**
   - What's the overall goal?
   - How does this chunk fit?
   - What's blocked/unblocked?

2. **Make judgment calls**
   - Agent suggests two approaches → Main session decides
   - Agent finds unexpected complexity → Main session re-scopes
   - Agent raises concern → Main session evaluates risk

3. **Coordinate across agents**
   - Agent A modified X, Agent B needs to know
   - Agent work might conflict → Main session resolves

4. **Quality gate enforcement**
   - Agent says "done" → Main session verifies
   - Tests pass → Main session confirms behavior preserved
   - Code looks good → Main session checks architecture fit

#### What to Preserve Between Sessions

When ending a work session, ensure these are captured:

```markdown
## Session Summary (template)

### Work Completed
- [ ] Item 1: Brief description
- [ ] Item 2: Brief description

### Decisions Made
- Decision: [what] → Reason: [why]

### Concerns/Risks Identified
- Area: [description of concern]

### Blocked Items
- Item: [what's blocked] → Needs: [what would unblock]

### Next Session Should
1. Start with: [specific task]
2. Consider: [thing to keep in mind]
3. Avoid: [known pitfall]
```

#### Example: Critical Thinking in Practice

```
Agent reports: "Extracted PrinterTemperatureState. All tests pass.
               Note: Found 3 places where temperature is set directly
               without going through the state class."

Main session critical thinking:
┌─────────────────────────────────────────────────────────────────────┐
│ 1. Tests pass - good, but what tests exist?                         │
│    → Ask agent to report test coverage for temperature methods      │
│                                                                     │
│ 2. "3 places setting directly" - this is a concern                  │
│    → These are potential bugs or intentional bypasses               │
│    → Need to understand each case before proceeding                 │
│    → Ask agent to list the 3 places and explain each               │
│                                                                     │
│ 3. Does this affect MoonrakerClient notifications?                  │
│    → Temperature updates come from WebSocket callbacks              │
│    → Main session knows this, agent might not have checked          │
│    → Ask agent to verify notification path still works              │
│                                                                     │
│ 4. Is the new class pattern consistent with our plan?               │
│    → Check: namespace, file location, naming convention             │
│    → Main session has context on what "consistent" means            │
└─────────────────────────────────────────────────────────────────────┘
```

### When NOT to Use Agents

Direct work (without agent delegation) is appropriate when:
- Single file, < 10 line change
- Exact location known, no investigation needed
- Simple mechanical change (rename, move)
- Fixing a typo or obvious bug

### Progress Sync Points

After each work session, update:
- [ ] Status checkbox in relevant section
- [ ] Implementation steps checkboxes
- [ ] Progress Tracking tables
- [ ] Change Log at bottom of document

---

## Tier 1: High-Priority Refactoring

### 1.1 PrinterState God Class Decomposition

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

#### Problem Statement
`PrinterState` is a 2808-line god class containing **98+ LVGL subjects** across 7+ unrelated domains. Every new feature touches this class, creating a bottleneck.

#### Current Structure
```
include/printer_state.h     (1297 lines)
src/printer/printer_state.cpp  (1511 lines)
```

#### Domains Currently Mixed
| Domain | Subjects | Description |
|--------|----------|-------------|
| Temperature | ~10 | Extruder/bed current/target, PID, firmware retraction |
| Motion | ~8 | X/Y/Z position, homed axes, speed/flow factors, Z-offset |
| Print State | ~15 | Progress, state enum, outcome, layers, duration, phases |
| Fan State | ~8 | Multi-fan discovery, per-fan speed subjects |
| LED State | ~5 | RGBW channels, brightness |
| Capabilities | ~20 | Printer capabilities, overrides, composite subjects |
| Hardware | ~10 | Validation results, issues, removal logic |
| Network | ~8 | Printer/Klippy/network status, messages |
| Excluded Objects | ~5 | Object names, active exclusions |
| AMS/Filament | ~10 | Filament sensor, AMS slots |

#### Proposed Split

```
PrinterState (coordinator/facade)
│
├── PrinterTemperatureState
│   ├── extruder_current_, extruder_target_
│   ├── bed_current_, bed_target_
│   ├── pid_kp_, pid_ki_, pid_kd_
│   └── firmware_retraction_*
│
├── PrinterMotionState
│   ├── x_pos_, y_pos_, z_pos_
│   ├── homed_axes_
│   ├── speed_factor_, flow_factor_
│   └── z_offset_, z_offset_delta_
│
├── PrinterPrintState
│   ├── print_state_, print_outcome_
│   ├── print_progress_, print_duration_
│   ├── print_filename_, print_message_
│   ├── current_layer_, total_layers_
│   └── print_start_phase_, print_start_message_
│
├── PrinterFanState
│   ├── fan_info_[]
│   ├── fan_speed_subjects_[]
│   └── part_cooling_fan_speed_
│
├── PrinterLEDState
│   ├── led_r_, led_g_, led_b_, led_w_
│   └── led_brightness_
│
├── PrinterCapabilitiesState
│   ├── capabilities_{}
│   ├── capability_overrides_{}
│   └── can_show_*, can_perform_* (composite subjects)
│
├── PrinterHardwareState
│   ├── hardware_validation_results_[]
│   ├── hardware_issues_label_
│   └── hardware_validation_complete_
│
└── PrinterNetworkState
    ├── klippy_state_, moonraker_state_
    ├── printer_message_
    └── is_connected_
```

#### Implementation Steps

**Phase 1: Characterization Tests (BEFORE any code changes)**
- [ ] Write characterization tests for temperature subject getters/setters
- [ ] Write characterization tests for motion subject getters/setters
- [ ] Write characterization tests for print state subject getters/setters
- [ ] Write characterization tests for fan state subject getters/setters
- [ ] Write characterization tests for LED state subject getters/setters
- [ ] Write characterization tests for capabilities subject getters/setters
- [ ] Write characterization tests for hardware state subject getters/setters
- [ ] Write characterization tests for network state subject getters/setters
- [ ] Write characterization tests for observer notification behavior
- [ ] Write characterization tests for async update patterns
- [ ] **✅ CHECKPOINT 1: Review and approve characterization tests**

**Phase 2: Structural Changes (one domain at a time)**
- [ ] Create `PrinterTemperatureState` class with extracted subjects
- [ ] Verify all characterization tests still pass
- [ ] **✅ Code review: PrinterTemperatureState**
- [ ] Create `PrinterMotionState` class with extracted subjects
- [ ] Verify all characterization tests still pass
- [ ] **✅ Code review: PrinterMotionState**
- [ ] Create `PrinterPrintState` class with extracted subjects
- [ ] Verify all characterization tests still pass
- [ ] **✅ Code review: PrinterPrintState**
- [ ] Create `PrinterFanState` class with extracted subjects
- [ ] Verify all characterization tests still pass
- [ ] **✅ Code review: PrinterFanState**
- [ ] Create `PrinterLEDState` class with extracted subjects
- [ ] Verify all characterization tests still pass
- [ ] **✅ Code review: PrinterLEDState**
- [ ] Create `PrinterCapabilitiesState` class with extracted subjects
- [ ] Verify all characterization tests still pass
- [ ] **✅ Code review: PrinterCapabilitiesState**
- [ ] Create `PrinterHardwareState` class with extracted subjects
- [ ] Verify all characterization tests still pass
- [ ] **✅ Code review: PrinterHardwareState**
- [ ] Create `PrinterNetworkState` class with extracted subjects
- [ ] Verify all characterization tests still pass
- [ ] **✅ Code review: PrinterNetworkState**

**Phase 3: Integration**
- [ ] Update `PrinterState` to compose these domain states
- [ ] Create forwarding accessors in `PrinterState` for backwards compatibility
- [ ] Verify all characterization tests still pass
- [ ] **✅ CHECKPOINT 2: Review integrated structure**

**Phase 4: Migration**
- [ ] Update UI panels to use new domain-specific accessors
- [ ] Deprecate old monolithic accessors
- [ ] Verify all tests (characterization + unit + integration) pass
- [ ] **✅ CHECKPOINT 3: Final review before merge**

**Phase 5: Cleanup**
- [ ] Remove deprecated accessors after migration complete
- [ ] Add new unit tests for domain state classes
- [ ] Update documentation

#### Files to Create
```
include/printer_temperature_state.h
include/printer_motion_state.h
include/printer_print_state.h
include/printer_fan_state.h
include/printer_led_state.h
include/printer_capabilities_state.h
include/printer_hardware_state.h
include/printer_network_state.h
src/printer/printer_temperature_state.cpp
src/printer/printer_motion_state.cpp
src/printer/printer_print_state.cpp
src/printer/printer_fan_state.cpp
src/printer/printer_led_state.cpp
src/printer/printer_capabilities_state.cpp
src/printer/printer_hardware_state.cpp
src/printer/printer_network_state.cpp
```

#### Files to Modify
```
include/printer_state.h          - Compose new domain states
src/printer/printer_state.cpp    - Delegate to domain states
src/ui/ui_panel_*.cpp           - Update subject access patterns
tests/unit/test_printer_state.cpp
```

#### Acceptance Criteria
- [ ] Each domain state class is independently testable
- [ ] No domain state class exceeds 400 lines
- [ ] UI panels can access state via domain-specific accessors
- [ ] Backwards-compatible accessors work during migration
- [ ] All existing tests pass
- [ ] New tests cover domain state classes

#### Risk Mitigation
- **High coupling**: Create forwarding accessors for gradual migration
- **Observer routing**: Subjects stay in domain state classes; observers don't need changes
- **Thread safety**: Each domain state handles its own async invokes

#### Metrics
| Metric | Before | After (Target) |
|--------|--------|----------------|
| printer_state.h lines | 1297 | <200 |
| printer_state.cpp lines | 1511 | <300 |
| Single file responsibilities | 7+ | 1 |
| Testable units | 1 | 9 |

---

### 1.2 Observer Pattern Factory

**Status**: [ ] Not Started  [x] In Progress  [ ] Complete

> **Progress**: Factory implemented + 3 pilot panels migrated. 6 files with 13 observers remaining.

#### Problem Statement
The same 15-line observer pattern is repeated **129+ times** across 85+ UI files, totaling approximately **2000 lines** of nearly identical boilerplate.

#### Current Pattern (repeated everywhere)
```cpp
observer_ = ObserverGuard(
    printer_state_.get_some_subject(),
    [](lv_observer_t* observer, lv_subject_t* subject) {
        auto* self = static_cast<PanelType*>(lv_observer_get_user_data(observer));
        if (self) {
            // Extract value based on subject type
            self->member_ = some_conversion(lv_subject_get_int(subject));
            // Always need async callback for UI updates
            ui_async_call([](void* ctx) {
                auto* panel = static_cast<PanelType*>(ctx);
                panel->update_method();
            }, self);
        }
    },
    this);
```

#### Proposed Solution
Create an observer factory with type-safe helpers:

```cpp
// include/observer_factory.h
namespace helix::ui {

// For simple value binding with update callback
template<typename T, typename Panel, typename Callback>
ObserverGuard create_value_observer(
    lv_subject_t* subject,
    T Panel::*member,
    Callback&& on_update,
    Panel* panel
);

// For simple value binding without callback
template<typename T, typename Panel>
ObserverGuard create_value_observer(
    lv_subject_t* subject,
    T Panel::*member,
    Panel* panel
);

// For custom transformation
template<typename T, typename Panel, typename Transform, typename Callback>
ObserverGuard create_transform_observer(
    lv_subject_t* subject,
    Transform&& transform,
    T Panel::*member,
    Callback&& on_update,
    Panel* panel
);

} // namespace helix::ui
```

#### Usage After Refactoring
```cpp
// Before (15 lines)
observer_ = ObserverGuard(
    printer_state_.get_extruder_temp_subject(),
    [](lv_observer_t* observer, lv_subject_t* subject) {
        auto* self = static_cast<FilamentPanel*>(lv_observer_get_user_data(observer));
        if (self) {
            self->nozzle_temp_ = lv_subject_get_int(subject) / 100;
            ui_async_call([](void* ctx) {
                auto* panel = static_cast<FilamentPanel*>(ctx);
                panel->update_temp_display();
            }, self);
        }
    },
    this);

// After (3 lines)
observer_ = create_value_observer(
    printer_state_.get_extruder_temp_subject(),
    &FilamentPanel::nozzle_temp_,
    &FilamentPanel::update_temp_display,
    this
);
```

#### Implementation Steps

**Phase 1: Characterization Tests (BEFORE any code changes)** ✅ COMPLETE
- [x] Catalog all observer pattern variations in use (int, float, string, pointer)
- [x] Write characterization tests for 3 pilot panels (FilamentPanel, ControlsPanel, ExtrusionPanel)
- [x] Document expected behavior: notification timing, value conversion, async safety
- [x] **✅ CHECKPOINT 1: Review and approve characterization tests**

**Phase 2: Factory Implementation** ✅ COMPLETE
- [x] Create `include/observer_factory.h` with template helpers (header-only, 466 lines)
- [x] Write unit tests for observer factory (12 tests, 36 assertions)
- [x] Verify factory tests pass
- [x] **✅ Code review: Observer factory implementation**

**Phase 3: Pilot Migration** ✅ COMPLETE
- [x] Migrate FilamentPanel to use factory (57% line reduction)
- [x] Verify FilamentPanel characterization tests pass
- [x] Migrate ControlsPanel to use factory (13 static callbacks removed)
- [x] Verify ControlsPanel characterization tests pass
- [x] Migrate ExtrusionPanel to use factory (1 static callback removed)
- [x] Verify ExtrusionPanel characterization tests pass
- [x] **✅ CHECKPOINT 2: Review pilot migration**

**Phase 4: Full Migration** ✅ COMPLETE
- [x] Migrate remaining 6 files with 13 observers:
  - [x] `ui_panel_motion.cpp` - 4 observers (position_x/y/z + bed_moves)
  - [x] `ui_panel_ams.cpp` - 2 observers (action, slot_count)
  - [x] `ui_fan_control_overlay.cpp` - 1 observer (fans_version)
  - [x] `ui_ams_slot.cpp` - 2 of 4 observers (color, status); kept current_slot/filament_loaded as traditional due to inter-dependencies
  - [x] `ui_ams_mini_status.cpp` - 2 observers (slot_count, slots_version)
  - [x] `ui_ams_current_tool.cpp` - 1 observer (color)
- [x] Run full test suite after each group
- [x] **✅ CHECKPOINT 3: Final review before merge**

**Phase 5: Cleanup**
- [ ] Remove old boilerplate patterns from all panels
- [ ] Update coding standards to require factory usage
- [ ] Document factory API in header

#### Files to Create
```
include/observer_factory.h
src/ui/observer_factory.cpp
tests/unit/test_observer_factory.cpp
tests/characterization/test_observer_patterns_char.cpp
```

#### Acceptance Criteria
- [ ] Factory handles int, float, string, pointer subject types
- [ ] Factory handles async/sync callbacks correctly
- [ ] Factory handles null checks and destruction safety
- [ ] All existing observer behavior is preserved
- [ ] Factory code compiles without template bloat warnings
- [ ] 80%+ of panels migrated to factory pattern

#### Metrics
| Metric | Before | After (Target) |
|--------|--------|----------------|
| Observer boilerplate lines | ~2000 | ~400 |
| Patterns to audit for thread safety | 129 | 1 |
| Lines per observer setup | 15 | 3-5 |

---

### 1.3 PrintStatusPanel Decomposition

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

#### Problem Statement
`PrintStatusPanel` is a 3782-line monster mixing:
- Print progress display
- Temperature control integration
- Speed/flow tuning
- Z-offset tuning (save/reset/adjustment)
- Excluded object management (with undo timer)
- Filament runout guidance
- Print cancel confirmation
- Print start progress overlay
- G-code viewer integration
- Layer tracking
- Thumbnail loading
- 4 modal subclasses defined inline

#### Current Structure
```
include/ui_panel_print_status.h     (857 lines)
  - Contains PrintCancelModal class definition
  - Contains SaveZOffsetModal class definition
  - Contains ExcludeObjectModal class definition
  - Contains RunoutGuidanceModal class definition
  - Contains PrintStatusPanel class definition

src/ui/ui_panel_print_status.cpp    (2925 lines)
  - All implementations mixed together
```

#### Proposed Split

```
PrintStatusPanel (core panel, ~800 lines)
├── Displays print progress, time, layers
├── Orchestrates overlays/modals
└── Observes core print state

PrintTuneOverlay (new, ~400 lines)
├── Speed factor adjustment
├── Flow factor adjustment
└── Callback handling

PrintZOffsetOverlay (new, ~500 lines)
├── Z-offset delta display
├── Baby-stepping controls
├── Save/reset modal integration
└── Delta accumulation logic

PrintExcludeObjectOverlay (new, ~400 lines)
├── Object list display
├── Exclude/include operations
├── Undo timer management
└── Modal integration

ui_print_status_modals.h (new, ~200 lines)
├── PrintCancelModal class
├── SaveZOffsetModal class
├── ExcludeObjectModal class
└── RunoutGuidanceModal class

FilamentRunoutOverlay (new, ~300 lines)
├── Runout detection display
├── Guidance UI
└── Resume controls
```

#### Implementation Steps

**Phase 1: Characterization Tests (BEFORE any code changes)**
- [ ] Write characterization tests for print progress display
- [ ] Write characterization tests for speed/flow tuning callbacks
- [ ] Write characterization tests for Z-offset tuning (delta tracking, save, reset)
- [ ] Write characterization tests for exclude object (select, exclude, undo)
- [ ] Write characterization tests for modal show/hide behavior
- [ ] Write characterization tests for filament runout guidance flow
- [ ] **✅ CHECKPOINT 1: Review and approve characterization tests**

**Phase 2: Extract Modals (lowest risk first)**
- [ ] Extract modal classes to `include/ui_print_status_modals.h`
- [ ] Extract modal implementations to `src/ui/ui_print_status_modals.cpp`
- [ ] Verify all characterization tests pass
- [ ] **✅ Code review: Modal extraction**

**Phase 3: Extract Overlays (one at a time)**
- [ ] Create `PrintTuneOverlay` class
- [ ] Verify characterization tests for tuning pass
- [ ] **✅ Code review: PrintTuneOverlay**
- [ ] Create `PrintZOffsetOverlay` class
- [ ] Verify characterization tests for Z-offset pass
- [ ] **✅ Code review: PrintZOffsetOverlay**
- [ ] Create `PrintExcludeObjectOverlay` class
- [ ] Verify characterization tests for exclude object pass
- [ ] **✅ Code review: PrintExcludeObjectOverlay**
- [ ] Create `FilamentRunoutOverlay` class
- [ ] Verify characterization tests for runout pass
- [ ] **✅ Code review: FilamentRunoutOverlay**

**Phase 4: Integration**
- [ ] Update `PrintStatusPanel` to orchestrate overlays
- [ ] Update navigation to register new overlays
- [ ] Verify all characterization tests pass
- [ ] **✅ CHECKPOINT 2: Review integrated structure**

**Phase 5: Manual Verification & Cleanup**
- [ ] Manual smoke test: start print, use all features
- [ ] Verify all print-time functionality works end-to-end
- [ ] Add new unit tests for each overlay
- [ ] **✅ CHECKPOINT 3: Final review before merge**

#### Files to Create
```
include/ui_print_status_modals.h
include/ui_print_tune_overlay.h
include/ui_print_zoffset_overlay.h
include/ui_print_exclude_object_overlay.h
include/ui_filament_runout_overlay.h
src/ui/ui_print_status_modals.cpp
src/ui/ui_print_tune_overlay.cpp
src/ui/ui_print_zoffset_overlay.cpp
src/ui/ui_print_exclude_object_overlay.cpp
src/ui/ui_filament_runout_overlay.cpp
tests/characterization/test_print_status_char.cpp
```

#### Files to Modify
```
include/ui_panel_print_status.h    - Remove modal classes, reduce scope
src/ui/ui_panel_print_status.cpp   - Remove extracted code
src/ui/ui_nav_manager.cpp          - Register new overlays
```

#### Acceptance Criteria
- [ ] PrintStatusPanel under 1000 lines
- [ ] Each overlay independently testable
- [ ] No functionality regression
- [ ] Modal behavior unchanged
- [ ] Z-offset operations work correctly
- [ ] Exclude object with undo works correctly

#### Risk Mitigation
- **Observer routing**: Each overlay observes only the subjects it needs
- **State coordination**: PrintStatusPanel coordinates overlay visibility
- **Modal lifecycle**: Modals move cleanly to separate file

#### Metrics
| Metric | Before | After (Target) |
|--------|--------|----------------|
| print_status.h lines | 857 | <300 |
| print_status.cpp lines | 2925 | <800 |
| Responsibilities | 10+ | 2 (display + orchestration) |
| Testable units | 1 | 6 |

---

### 1.4 SubjectManagedPanel Universal Adoption

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

#### Problem Statement
`SubjectManagedPanel` base class exists to manage subject lifecycle but has **<50% adoption**. The remaining panels implement ~135-216 lines each of duplicated init/deinit boilerplate, totaling **~4000 lines** of duplicated code.

#### Current Pattern (repeated in 27+ panels)
```cpp
void Panel::init_subjects() {
    if (subjects_initialized_) return;

    std::snprintf(subject1_name_, sizeof(subject1_name_), "%s_%s", name_, "subject1");
    lv_subject_init_int(&subject1_, 0);
    lv_xml_register_subject(subject1_name_, &subject1_);
    static_subject_registry_.register_subject(subject1_name_, &subject1_);

    // Repeat 5-15 more times...

    subjects_initialized_ = true;
}

void Panel::deinit_subjects() {
    if (!subjects_initialized_) return;

    lv_subject_deinit(&subject1_);
    lv_subject_deinit(&subject2_);
    // Repeat for all subjects...

    subjects_initialized_ = false;
}
```

#### Target Pattern
```cpp
class FilamentPanel : public SubjectManagedPanel {
private:
    // Subjects managed automatically by base class
    ManagedSubject<int> nozzle_temp_{this, "nozzle_temp", 0};
    ManagedSubject<int> bed_temp_{this, "bed_temp", 0};
    ManagedSubject<std::string> status_{this, "status", ""};

    // init_subjects() and deinit_subjects() handled by base class
};
```

#### Panels to Migrate
| Panel | Subject Count | Priority |
|-------|---------------|----------|
| ControlsPanel | 27 | High |
| FilamentPanel | 16 | High |
| PrintStatusPanel | 20 | High |
| BedMeshPanel | 15 | High |
| ExtrusionPanel | 6 | Medium |
| SettingsPanel | 12 | Medium |
| MotionPanel | 8 | Medium |
| PrintSelectPanel | 10 | Medium |
| HomePanel | 5 | Low |
| *17 other panels* | Variable | Low |

#### Implementation Steps

**Phase 1: Characterization Tests & Audit**
- [ ] Audit `SubjectManagedPanel` implementation for completeness
- [ ] Write characterization tests for subject lifecycle in 4 high-priority panels
- [ ] Document subject init/deinit order dependencies
- [ ] **✅ CHECKPOINT 1: Review audit results and characterization tests**

**Phase 2: Infrastructure**
- [ ] Add `ManagedSubject<T>` template if not already present
- [ ] Write unit tests for ManagedSubject
- [ ] Create migration guide document
- [ ] **✅ Code review: Infrastructure additions**

**Phase 3: High-Priority Panel Migration**
- [ ] Migrate ControlsPanel (highest subject count)
- [ ] Verify ControlsPanel characterization tests pass
- [ ] **✅ Code review: ControlsPanel**
- [ ] Migrate FilamentPanel
- [ ] Verify FilamentPanel characterization tests pass
- [ ] **✅ Code review: FilamentPanel**
- [ ] Migrate PrintStatusPanel
- [ ] Verify PrintStatusPanel characterization tests pass
- [ ] **✅ Code review: PrintStatusPanel**
- [ ] Migrate BedMeshPanel
- [ ] Verify BedMeshPanel characterization tests pass
- [ ] **✅ Code review: BedMeshPanel**
- [ ] **✅ CHECKPOINT 2: Review high-priority migrations**

**Phase 4: Remaining Panels**
- [ ] Migrate remaining medium-priority panels (one at a time, verify tests)
- [ ] Migrate remaining low-priority panels
- [ ] Run full test suite
- [ ] **✅ CHECKPOINT 3: Final review before merge**

**Phase 5: Cleanup**
- [ ] Remove manual subject management from migrated panels
- [ ] Update coding standards to require SubjectManagedPanel
- [ ] Grep codebase for any remaining manual subject patterns

#### Acceptance Criteria
- [ ] All panels with subjects inherit from SubjectManagedPanel
- [ ] No manual `lv_subject_init()` calls in panel code
- [ ] No manual `lv_subject_deinit()` calls in panel code
- [ ] No `subjects_initialized_` flags in panel code
- [ ] All subject registration automatic
- [ ] All tests pass

#### Metrics
| Metric | Before | After (Target) |
|--------|--------|----------------|
| Manual init/deinit boilerplate | ~4000 lines | 0 |
| Panels with manual subject management | 27+ | 0 |
| Subject lifecycle bugs possible | High | Low |

---

### 1.5 MoonrakerAPI Domain Split

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

#### Problem Statement
`MoonrakerAPI` is a 1190-line facade containing **70+ public methods** across 5+ unrelated domains. Finding the right method requires scrolling through a massive interface.

#### Current Structure
```
include/moonraker_api.h    (1190 lines)
  - 70+ public methods
  - 15+ callback type aliases
  - File operations, job control, motion, heating, macros,
    filament, AMS, bed mesh, history, timelapse, Spoolman
    all in one class
```

#### Proposed Split

```
MoonrakerAPI (facade, ~200 lines)
├── Composes domain APIs
├── Provides unified initialization
└── Backwards-compatible accessors

MoonrakerFileAPI (~150 lines)
├── list_files(), get_file_metadata()
├── delete_file(), move_file(), copy_file()
├── create_directory(), delete_directory()
└── File upload progress callbacks

MoonrakerJobAPI (~150 lines)
├── start_print(), pause_print(), resume_print(), cancel_print()
├── set_speed_factor(), set_flow_factor()
├── set_z_offset()
└── Job state callbacks

MoonrakerMotionAPI (~100 lines)
├── home_axes(), move_axis()
├── query_endstops(), query_position()
├── enable_motors(), disable_motors()
└── Emergency stop

MoonrakerHeatingAPI (~100 lines)
├── set_extruder_temp(), set_bed_temp()
├── run_pid_tune(), save_pid()
├── Firmware retraction settings
└── Preheat presets

MoonrakerMacroAPI (~100 lines)
├── run_macro(), list_macros()
├── get_macro_params()
└── Macro execution callbacks

MoonrakerFilamentAPI (~100 lines)
├── load_filament(), unload_filament()
├── Filament sensor queries
└── Runout callbacks

MoonrakerHistoryAPI (~100 lines)
├── get_print_history()
├── get_job_totals()
└── History entry callbacks
```

#### Implementation Steps

**Phase 1: Characterization Tests (BEFORE any code changes)**
- [ ] Catalog all MoonrakerAPI methods by domain
- [ ] Write characterization tests for file operations (list, delete, move, copy)
- [ ] Write characterization tests for job control (start, pause, resume, cancel)
- [ ] Write characterization tests for motion control (home, move, queries)
- [ ] Write characterization tests for heating operations
- [ ] Write characterization tests for macro operations
- [ ] Write characterization tests for filament operations
- [ ] Write characterization tests for history queries
- [ ] **✅ CHECKPOINT 1: Review and approve characterization tests**

**Phase 2: Domain API Extraction (one at a time)**
- [ ] Create `MoonrakerFileAPI` with file operations
- [ ] Verify file operation characterization tests pass
- [ ] **✅ Code review: MoonrakerFileAPI**
- [ ] Create `MoonrakerJobAPI` with job control
- [ ] Verify job control characterization tests pass
- [ ] **✅ Code review: MoonrakerJobAPI**
- [ ] Create `MoonrakerMotionAPI` with motion control
- [ ] Verify motion characterization tests pass
- [ ] **✅ Code review: MoonrakerMotionAPI**
- [ ] Create `MoonrakerHeatingAPI` with temperature operations
- [ ] Verify heating characterization tests pass
- [ ] **✅ Code review: MoonrakerHeatingAPI**
- [ ] Create `MoonrakerMacroAPI` with macro operations
- [ ] Verify macro characterization tests pass
- [ ] **✅ Code review: MoonrakerMacroAPI**
- [ ] Create `MoonrakerFilamentAPI` with filament operations
- [ ] Verify filament characterization tests pass
- [ ] **✅ Code review: MoonrakerFilamentAPI**
- [ ] Create `MoonrakerHistoryAPI` with history queries
- [ ] Verify history characterization tests pass
- [ ] **✅ Code review: MoonrakerHistoryAPI**

**Phase 3: Integration**
- [ ] Update `MoonrakerAPI` to compose domain APIs
- [ ] Create forwarding methods for backwards compatibility
- [ ] Verify all characterization tests pass
- [ ] **✅ CHECKPOINT 2: Review integrated structure**

**Phase 4: Migration**
- [ ] Update callers to use domain-specific APIs
- [ ] Deprecate monolithic accessors
- [ ] Run full test suite
- [ ] **✅ CHECKPOINT 3: Final review before merge**

**Phase 5: Cleanup**
- [ ] Remove deprecated accessors after migration complete
- [ ] Add new unit tests for domain APIs
- [ ] Update API documentation

#### Files to Create
```
include/moonraker_file_api.h
include/moonraker_job_api.h
include/moonraker_motion_api.h
include/moonraker_heating_api.h
include/moonraker_macro_api.h
include/moonraker_filament_api.h
include/moonraker_history_api.h
src/api/moonraker_file_api.cpp
src/api/moonraker_job_api.cpp
src/api/moonraker_motion_api.cpp
src/api/moonraker_heating_api.cpp
src/api/moonraker_macro_api.cpp
src/api/moonraker_filament_api.cpp
src/api/moonraker_history_api.cpp
tests/characterization/test_moonraker_api_char.cpp
```

#### Acceptance Criteria
- [ ] Each domain API is independently usable
- [ ] No domain API exceeds 200 lines
- [ ] Callers can use domain-specific or facade interface
- [ ] All callbacks work correctly
- [ ] All existing tests pass
- [ ] New tests cover domain APIs

#### Metrics
| Metric | Before | After (Target) |
|--------|--------|----------------|
| moonraker_api.h lines | 1190 | <200 |
| Methods per interface | 70+ | <15 |
| Domains mixed | 7 | 1 |
| Testable units | 1 | 8 |

---

## Tier 2: Medium-Priority Refactoring

### 2.1 Unified Error Handling with Result<T>

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

#### Problem Statement
Three incompatible error patterns exist:
1. `MoonrakerError` struct with type classification
2. `AmsError` struct (independent implementation)
3. Raw `bool`/`std::string` returns

#### Proposed Solution
```cpp
// include/result.h
namespace helix {

template<typename T>
class Result {
public:
    // Success construction
    static Result<T> success(T value);

    // Error construction
    static Result<T> error(std::string message);
    static Result<T> error(ErrorType type, std::string message);
    static Result<T> error(ErrorType type, std::string message, nlohmann::json details);

    // Access
    bool is_success() const;
    bool is_error() const;
    T& value();
    const T& value() const;
    const Error& error() const;

    // Monadic operations
    template<typename F>
    auto map(F&& f) -> Result<decltype(f(std::declval<T>()))>;

    template<typename F>
    auto flat_map(F&& f) -> decltype(f(std::declval<T>()));
};

} // namespace helix
```

#### Implementation Steps
- [ ] Create `include/result.h` with Result<T> template
- [ ] Create `include/error.h` with unified Error struct
- [ ] Migrate MoonrakerAPI methods to return Result<T>
- [ ] Migrate AMS operations to return Result<T>
- [ ] Update UI code to handle Result<T>
- [ ] Remove MoonrakerError and AmsError types
- [ ] Update tests

#### Acceptance Criteria
- [ ] All async operations return Result<T>
- [ ] Error types are unified
- [ ] UI displays errors consistently
- [ ] No raw bool/string error returns

---

### 2.2 Namespace Organization

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

#### Problem Statement
Inconsistent namespace usage:
- `helix::` for some components
- `helix::ui::` for some UI components
- Global namespace for most panels and PrinterState
- No clear convention

#### Proposed Structure
```
helix::
├── core::
│   ├── PrinterState
│   ├── Application
│   └── Config
├── ui::
│   ├── panels::
│   │   ├── HomePanel
│   │   ├── ControlsPanel
│   │   └── ...
│   ├── overlays::
│   └── modals::
├── api::
│   ├── MoonrakerAPI
│   ├── MoonrakerClient
│   └── Domain APIs
├── domain::
│   ├── Temperature
│   ├── PrintJob
│   └── ...
└── util::
    ├── async helpers
    └── string utilities
```

#### Implementation Steps
- [ ] Document namespace conventions
- [ ] Create namespace migration guide
- [ ] Add namespace to new files
- [ ] Migrate core files (PrinterState, Application)
- [ ] Migrate API files
- [ ] Migrate UI panels (largest effort)
- [ ] Update all includes
- [ ] Remove using declarations at file scope

---

### 2.3 Dependency Injection for Panels

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

#### Problem Statement
Panels access 4-5 singletons per method, creating hidden dependencies and making testing difficult.

#### Current Pattern
```cpp
void SettingsPanel::on_some_action() {
    SettingsManager::instance().save();
    PrinterState::get_printer_state().set_something();
    NavigationManager::instance().go_back();
    MoonrakerAPI* api = get_moonraker_api();
    api->do_something();
}
```

#### Proposed Pattern
```cpp
struct PanelDependencies {
    PrinterState& printer;
    SettingsManager& settings;
    MoonrakerAPI* api;
    NavigationManager& nav;
    // Add more as needed
};

class SettingsPanel : public PanelBase {
public:
    explicit SettingsPanel(const PanelDependencies& deps);

private:
    PanelDependencies deps_;
};

// Usage
void SettingsPanel::on_some_action() {
    deps_.settings.save();
    deps_.printer.set_something();
    deps_.nav.go_back();
    deps_.api->do_something();
}
```

#### Implementation Steps
- [ ] Create `PanelDependencies` struct
- [ ] Update PanelBase to accept dependencies
- [ ] Update panel factory to inject dependencies
- [ ] Migrate panels incrementally
- [ ] Update tests to inject mock dependencies
- [ ] Remove singleton access from panels

---

### 2.4 MoonrakerClient Decomposition

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

#### Problem Statement
`MoonrakerClient` (808 lines header, 1618 lines impl) mixes:
- WebSocket connection management
- JSON-RPC protocol handling
- Subscription management
- Request tracking
- Reconnection logic
- Discovery caching
- Notification routing

#### Proposed Split
```
MoonrakerClient (orchestrator, ~300 lines)
├── MoonrakerConnection (~400 lines)
│   ├── WebSocket setup
│   ├── Connect/disconnect
│   └── Reconnection logic
├── MoonrakerProtocol (~300 lines)
│   ├── JSON-RPC encoding/decoding
│   ├── Request ID tracking
│   └── Response routing
├── MoonrakerSubscriptions (~200 lines)
│   ├── Subscription management
│   └── Notification callbacks
└── MoonrakerDiscoveryCache (~200 lines)
    ├── Hostname caching
    ├── Heater/fan/LED discovery
    └── MCU list caching
```

---

### 2.5 Controls Panel Z-Offset Extraction

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

#### Problem Statement
`ui_panel_controls.cpp` (1658 lines) contains Z-offset calibration logic that overlaps with PrintStatusPanel's Z-offset tuning. These should be unified or at least share common components.

#### Proposed Solution
- Extract `ZOffsetCalibrationPanel` for manual probe workflow
- Extract `ZOffsetBabystepOverlay` for live baby-stepping
- Create shared `ZOffsetUtils` for common calculations
- Keep `MotionControlPanel` for basic motion only

---

## Quick Wins

### Temperature Formatting Utilities

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

**Problem**: 43+ occurrences of temperature formatting:
```cpp
std::snprintf(buf, sizeof(buf), "%d / %d°C", current, target);
```

**Solution**:
```cpp
// include/ui_temperature_utils.h
namespace helix::ui {
    std::string format_temp_pair(int current, int target);
    std::string format_temp_current(int current);
    std::string format_temp_target(int target);
}
```

**Effort**: 2 hours | **Lines saved**: ~200

---

### AutoModal RAII Wrapper

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

**Problem**: 74+ occurrences of modal cleanup:
```cpp
if (lv_is_initialized()) {
    if (modal_) { ui_modal_hide(modal_); modal_ = nullptr; }
}
```

**Solution**:
```cpp
// include/auto_modal.h
class AutoModal {
    lv_obj_t* modal_ = nullptr;
public:
    ~AutoModal() { hide(); }
    void show(lv_obj_t* modal) { hide(); modal_ = modal; }
    void hide() { if (modal_) { ui_modal_hide(modal_); modal_ = nullptr; } }
};
```

**Effort**: 3 hours | **Lines saved**: ~400

---

### Test Stub Consolidation

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

**Problem**: 15+ stub pairs with identical logging pattern:
```cpp
void ui_notification_info(const char* message) { spdlog::debug("[Test Stub] info: {}", message); }
void ui_notification_info(const char* title, const char* message) { spdlog::debug("[Test Stub] info: {} - {}", title, message); }
void ui_notification_success(const char* message) { ... }
void ui_notification_success(const char* title, const char* message) { ... }
// etc.
```

**Solution**:
```cpp
#define STUB_NOTIFY_1(name) \
    void ui_notification_##name(const char* msg) { \
        spdlog::debug("[Test Stub] " #name ": {}", msg); \
    }
#define STUB_NOTIFY_2(name) \
    void ui_notification_##name(const char* title, const char* msg) { \
        spdlog::debug("[Test Stub] " #name ": {} - {}", title, msg); \
    }
```

**Effort**: 2 hours | **Lines saved**: ~150

---

### Parent Coordinate Utility

**Status**: [ ] Not Started  [ ] In Progress  [ ] Complete

**Problem**: 2 occurrences of parent traversal:
```cpp
lv_obj_t* parent = lv_obj_get_parent(widget);
while (parent) {
    x += lv_obj_get_x(parent);
    y += lv_obj_get_y(parent);
    parent = lv_obj_get_parent(parent);
}
```

**Solution**: Extract to `get_absolute_position()` in `ui_utils.h`

**Effort**: 30 minutes | **Lines saved**: ~10

---

## Implementation Phases

### Phase 1: Quick Wins (1-2 days)
| Item | Effort | Owner | Status |
|------|--------|-------|--------|
| Temperature formatting utilities | 2h | | [ ] |
| AutoModal RAII wrapper | 3h | | [ ] |
| Test stub consolidation | 2h | | [ ] |
| Parent coordinate utility | 30m | | [ ] |
| Extract PrintStatusPanel modals | 2h | | [ ] |

### Phase 2: Foundation (1 week)
| Item | Effort | Owner | Status |
|------|--------|-------|--------|
| Observer factory implementation | 1d | | [x] Complete |
| Observer factory pilot migration | 1d | | [x] Complete (3 panels) |
| Observer factory full migration | 0.5d | | [ ] 6 files remaining |
| SubjectManagedPanel audit | 0.5d | | [ ] |
| SubjectManagedPanel high-priority migration | 2d | | [ ] |

### Phase 3: Architecture (2-3 weeks)
| Item | Effort | Owner | Status |
|------|--------|-------|--------|
| PrinterState domain split - design | 1d | | [ ] |
| PrinterState domain split - implementation | 3d | | [ ] |
| PrinterState migration | 2d | | [ ] |
| MoonrakerAPI domain split | 2d | | [ ] |
| PrintStatusPanel decomposition | 3d | | [ ] |

### Phase 4: Polish (ongoing)
| Item | Effort | Owner | Status |
|------|--------|-------|--------|
| Unified error handling | 3d | | [ ] |
| Namespace organization | 5d | | [ ] |
| Dependency injection | 5d | | [ ] |
| MoonrakerClient decomposition | 3d | | [ ] |

---

## Progress Tracking

### Overall Progress
| Phase | Items | Complete | Progress |
|-------|-------|----------|----------|
| Phase 1: Quick Wins | 5 | 0 | 0% |
| Phase 2: Foundation | 4 | 0 | 0% |
| Phase 3: Architecture | 5 | 0 | 0% |
| Phase 4: Polish | 4 | 0 | 0% |
| **Total** | **18** | **0** | **0%** |

### Metrics Dashboard
| Metric | Current | Target | Progress |
|--------|---------|--------|----------|
| Lines of duplicated code | ~8000 | <1000 | 0% |
| PrinterState lines | 2808 | <500 | 0% |
| PrintStatusPanel lines | 3782 | <1000 | 0% |
| Panels using SubjectManagedPanel | ~50% | 100% | 50% |
| Observer boilerplate instances | ~90 | <20 | 30% |
| Modal cleanup boilerplate instances | 74 | <10 | 0% |

> **Note (2026-01-09)**: Observer Factory Phase 4 complete. Migrated ~39 observers across 9 files
> (3 pilot panels + 6 Phase 4 files) using `observe_int_sync<T>()` lambda pattern.

---

## Appendix: Code Examples

### A.1 Observer Factory Template Implementation

```cpp
// include/observer_factory.h
#pragma once

#include "observer_guard.h"
#include "ui_update_queue.h"
#include <lvgl.h>

namespace helix::ui {

namespace detail {
    template<typename T>
    T get_subject_value(lv_subject_t* subject) {
        if constexpr (std::is_same_v<T, int>) {
            return lv_subject_get_int(subject);
        } else if constexpr (std::is_same_v<T, float>) {
            // Handle float subjects
            return static_cast<float>(lv_subject_get_int(subject)) / 100.0f;
        } else if constexpr (std::is_pointer_v<T>) {
            return static_cast<T>(lv_subject_get_pointer(subject));
        }
    }
}

template<typename T, typename Panel, typename Callback>
ObserverGuard create_value_observer(
    lv_subject_t* subject,
    T Panel::*member,
    Callback&& on_update,
    Panel* panel)
{
    return ObserverGuard(
        subject,
        [member, on_update](lv_observer_t* obs, lv_subject_t* subj) {
            auto* self = static_cast<Panel*>(lv_observer_get_user_data(obs));
            if (!self) return;

            self->*member = detail::get_subject_value<T>(subj);

            ui_async_call([on_update](void* ctx) {
                auto* p = static_cast<Panel*>(ctx);
                (p->*on_update)();
            }, self);
        },
        panel
    );
}

} // namespace helix::ui
```

### A.2 ManagedSubject Template

```cpp
// include/managed_subject.h
#pragma once

#include <lvgl.h>
#include <string>

namespace helix::ui {

template<typename T>
class ManagedSubject {
public:
    ManagedSubject(SubjectManagedPanel* owner, const char* name, T initial);
    ~ManagedSubject();

    lv_subject_t* get() { return &subject_; }
    T value() const;
    void set(T value);

private:
    lv_subject_t subject_;
    std::string registered_name_;
    SubjectManagedPanel* owner_;
};

} // namespace helix::ui
```

### A.3 Result<T> Template

```cpp
// include/result.h
#pragma once

#include <variant>
#include <string>
#include <nlohmann/json.hpp>

namespace helix {

enum class ErrorType {
    Network,
    Protocol,
    Validation,
    NotFound,
    Timeout,
    Unknown
};

struct Error {
    ErrorType type;
    std::string message;
    nlohmann::json details;
};

template<typename T>
class Result {
public:
    static Result<T> success(T value) {
        Result r;
        r.data_ = std::move(value);
        return r;
    }

    static Result<T> error(ErrorType type, std::string message) {
        Result r;
        r.data_ = Error{type, std::move(message), {}};
        return r;
    }

    bool is_success() const { return std::holds_alternative<T>(data_); }
    bool is_error() const { return std::holds_alternative<Error>(data_); }

    T& value() { return std::get<T>(data_); }
    const T& value() const { return std::get<T>(data_); }
    const Error& error() const { return std::get<Error>(data_); }

private:
    std::variant<T, Error> data_;
};

} // namespace helix
```

---

## Change Log

| Date | Author | Changes |
|------|--------|---------|
| 2026-01-08 | Claude | Initial document creation |
| 2026-01-09 | Claude | Observer factory: Phase 1-3 complete. Factory implemented (466 lines, 12 tests), 3 pilot panels migrated (FilamentPanel, ControlsPanel, ExtrusionPanel). Net -146 lines. 6 files/13 observers remaining. |
