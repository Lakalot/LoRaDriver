# ADR 0003: IRQ Boundary and Event Pipeline

## Status

Accepted

## Context

ISR paths must remain bounded, deterministic, and allocation-free.

## Decision

ISR captures signals and enqueues fixed-size events only; processing runs outside ISR.

## Consequences

- Predictable timing and reduced ISR risk.
- Queue boundary behavior can be tested deterministically.
