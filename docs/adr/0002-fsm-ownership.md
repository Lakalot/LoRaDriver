# ADR 0002: Single FSM Ownership

## Status

Accepted

## Context

Deterministic behavior requires a single authority over mutable runtime state.

## Decision

Core FSM owns mutable state. Chip/platform adapters cannot directly mutate FSM state.

## Consequences

- Easier reasoning about transitions and incident replay.
- Adapter boundaries remain strict and testable.
- TX/RX lifecycle paths remain explicit and deterministic (`Ready -> TxPreparing -> TxInProgress -> TxCompleted/TxFailed -> Ready`, plus deterministic RX return-to-listen).
