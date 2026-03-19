# V1 Support Boundaries

## Supported Profiles

- Chips: SX1276, SX1278
- Bands: 433 MHz, 868 MHz
- IRQ routing: DIO0 only, DIO0 + DIO1

## Deferred in V1

- SX126x runtime support is deferred and represented as stub-only.
- Entry criteria for SX126x onboarding are defined in [`docs/scope/v1-bis-entry-criteria.md`](v1-bis-entry-criteria.md).

## Protocol Scope

- In scope: LoRa P2P
- Out of scope: LoRaWAN

## Related Documentation

- Power profile and DIO wiring trade-offs: [`docs/scope/power-profile-comparison.md`](power-profile-comparison.md)
- V1-bis onboarding prerequisites: [`docs/scope/v1-bis-entry-criteria.md`](v1-bis-entry-criteria.md)
- Integration guide: [`docs/api/integration-guide.md`](../api/integration-guide.md)
