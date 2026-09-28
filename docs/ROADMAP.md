# Roadmap

## P0 — Native core

- [x] C++20 project baseline
- [x] canonical value/quality/time types
- [x] service taxonomy
- [x] bounded session state
- [x] worker abstraction
- [x] bounded trace buffer
- [x] unit-test baseline

## P1 — Preliminary DMS BER profile

- [ ] pin public preliminary ASN.1 schema revision
- [ ] schema provenance manifest + SHA-256
- [ ] TpaaPdu canonical model
- [ ] BER decoder with strict length/depth limits
- [ ] BER encoder
- [ ] associate / release / abort
- [ ] malformed-message corpus
- [ ] Netbeheer golden vectors

Exit: byte-compatible association exchange with the pinned reference endpoint.

## P2 — Discovery and data access

- [ ] WebSocket transport adapter
- [ ] client/server roles independent of active/passive WS role
- [ ] GetServerDirectory
- [ ] GetLogicalDeviceDirectory
- [ ] GetLogicalNodeDirectory
- [ ] GetDataDirectory
- [ ] GetDataDefinition
- [ ] GetDataValues
- [ ] model-tree builder

Exit: native client discovers the Netbeheer FT20 model and native server is discoverable by the Netbeheer client.

## P3 — DataSets and reporting

- [ ] DataSet directory and values
- [ ] RCB state model
- [ ] Get/Set URCB and BRCB
- [ ] GI state/evidence
- [ ] trigger options
- [ ] unconfirmed report decode/encode
- [ ] event-driven update path
- [ ] sequence/confRev continuity checks

Exit: a simulator value change reaches the client through a real report with no cyclic read fallback.

## P4 — Lab simulator and workbench API

- [ ] headless server CLI
- [ ] headless client CLI
- [ ] simulator model API
- [ ] deterministic scenario runner
- [ ] trace streaming API
- [ ] Workbench integration

## P5 — Standard evolution

- [ ] JER codec
- [ ] DER codec
- [ ] schema/version negotiation strategy
- [ ] security profile research
- [ ] conformance-test mapping when stable public procedures exist
