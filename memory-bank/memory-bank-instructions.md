# Memory Bank Instructions

This folder stores persistent notes, context, and progress tracking across all sessions.

## Folder Structure

```
memory-bank/
├── memory-bank-instructions.md (this file)
├── activeContext.md            (current session state)
├── progress.md                 (phase/task progress)
└── phase-4/                    (feature-specific notes)
```

## Files & Update Rules

### activeContext.md
- **Purpose:** Current session state, active tasks, immediate focus
- **When to update:** At session start and end
- **Scope:** What's being worked on NOW
- **Retention:** Overwrite with latest session info (not cumulative)

### progress.md
- **Purpose:** Phase/task completion tracking
- **When to update:** After completing any task or at phase boundaries
- **Scope:** Overall project progress across all phases
- **Retention:** Append new entries, keep cumulative history

### phase-N/ folders
- **Purpose:** Feature-specific implementation notes, patterns, lessons learned
- **When to update:** As features are completed or patterns discovered
- **Scope:** Deep dives into architecture, gotchas, working solutions
- **Retention:** Permanent reference for future sessions

## How Copilot Uses Memory Bank

1. **Before task start:** Read memory-bank-instructions.md + activeContext.md + progress.md
2. **During task:** Consult relevant phase folder for patterns/gotchas
3. **At task end:** Update activeContext.md with completion notes
4. **At phase boundary:** Add new entry to progress.md with phase recap

## Key Principle

Memory bank is NOT a logbook. It's a knowledge base for:
- **What patterns work** (e.g., icon visual feedback = relay-bound vs control logic-bound)
- **What doesn't work** (e.g., meter widget indicators can't update dynamically)
- **Current status** (which phase, what's pending, next steps)
- **Quick reference** (GPIO assignments, color palette, build constraints)

Keep entries concise. Link to detailed docs in `reference/` folder.
