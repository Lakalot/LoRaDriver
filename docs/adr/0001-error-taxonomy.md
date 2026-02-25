# ADR 0001: Error Taxonomy Baseline

## Status

Accepted

## Context

V1 requires typed, deterministic error handling with no runtime exceptions.

## Decision

Use `LoRaError` as the public typed status for fallible operations.

For deterministic initialization in V1 scope, classify startup failures explicitly as invalid configuration, unsupported profile,
hardware initialization failure, transition guard failure, and already initialized violations.

Expose a minimum typed diagnostic context for the latest failure (`error`, `detail_code`, `chip`, `band`, `dio_routing`) so
unsupported profile and startup phase failures are operationally diagnosable without ad-hoc log parsing.

## Consequences

- Public API remains explicit and machine-checkable.
- Error growth will be centralized and versioned.
- Startup failures are diagnosable through typed outcomes plus a minimum diagnostic detail code.
