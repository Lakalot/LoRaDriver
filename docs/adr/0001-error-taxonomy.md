# ADR 0001: Error Taxonomy Baseline

## Status

Accepted

## Context

V1 requires typed, deterministic error handling with no runtime exceptions.

## Decision

Use `LoRaError` as the public typed status for fallible operations.

## Consequences

- Public API remains explicit and machine-checkable.
- Error growth will be centralized and versioned.
