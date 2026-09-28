# Netbeheer engine audit and native redesign notes

Reference baseline: `Netbeheer-Nederland/iec61850-websocket` at commit
`f78b0d55780aecab551fb5fd1e75e916bc120215`.

The Netbeheer project is valuable as a public proof-of-concept and interoperability oracle for the preliminary IEC 61850-8-3 work. This document records engineering observations that guide this repository's independent native implementation. It is not a criticism of the reference project and does not imply defects or non-conformance.

## What the reference implementation demonstrates well

- a concrete TPAA message model backed by a public ASN.1 schema;
- both JER and BER experiments through a common schema;
- WebSocket active and passive endpoint modes;
- association, discovery, data access, DataSet, RCB, reporting and control experiments;
- security experiments around TLS/OAuth;
- practical FT20 client/server scenarios that are useful as interoperability evidence.

Those behaviors give this project a useful external oracle while IEC 61850-8-3 is still evolving.

## Native redesign decisions

### 1. Schema compilation is moved out of the runtime hot path

The reference implementation compiles the ASN.1 schema with `asn1tools` when its encode/decode module is imported.

Our engine instead uses a native typed codec boundary with bounded BER parsing. The runtime does not require Python, `asn1tools`, or dynamic schema compilation.

Future generated code is acceptable if generation happens at build time and the generated artifact is reviewed/pinned. The runtime remains native.

### 2. Dynamic tuple/dictionary PDUs become typed canonical structures

The reference project represents decoded protocol objects primarily as Python tuples/dictionaries.

Our implementation exposes typed C++ structures such as:

- `DmsPdu`;
- `AssociateRequest` / `AssociateResponse`;
- directory request/response types;
- canonical IEC data, quality and timestamp types;
- typed report and RCB state.

This provides compile-time service boundaries and avoids repeated string-key lookups on the protocol path.

### 3. IEC service role is separated from WebSocket transport role

In the reference FT20 profile, an IEC 61850 client can be the passive WebSocket listener while the IEC 61850 server actively connects to it. That is valid but can be confusing when transport role and IEC role are represented by the same object lifecycle.

Our architecture treats these independently:

```text
IEC role:       Client | Server
Transport role: Active | Passive
```

Neither implies the other.

### 4. Request correlation uses a bounded keyed session table

The reference client uses a deque plus event notifications and repeated scans to locate matching responses.

Our `Session` owns a bounded invoke-ID map. Each outstanding request records service identity and creation time. The design target is O(1)-style keyed completion rather than scanning a dynamic response list.

Timeout and cancellation policy will be layered on this state model rather than embedded separately in every service function.

### 5. Service dispatch is decomposed

The reference server contains a broad request dispatcher with service-specific logic in one asynchronous request path.

Our target is:

```text
wire -> bounded BER codec -> typed DmsPdu
     -> association/session validation
     -> service registry/handler
     -> canonical model
     -> typed response
```

The current `ServerCore` is intentionally small. As coverage grows, service families will be separated into discovery, data, DataSet, reporting and control handlers.

### 6. Model mutation is serialized and independent of UI

The simulator model will be mutated through a dedicated model worker/command path.

The browser, protocol inspector and HTTP control API are observers/controllers. They do not own the IEC model and cannot block the service worker.

### 7. Reporting is push-first

A real IEC report becomes a typed `ReportEvent`.

The browser receives report events from the engine's observer stream. Cyclic `GetDataValues` polling is a separate, explicit monitoring feature and is never used as a silent reporting fallback.

### 8. Resource limits are explicit

The preliminary BER reader rejects:

- indefinite BER lengths;
- malformed/non-minimal long lengths;
- malformed high-tag encodings;
- truncated TLVs;
- configured message/TLV size overflow;
- excessive child count;
- excessive nesting depth.

Outstanding requests, trace storage and worker queues are bounded as well.

### 9. Trace evidence is first-class

Wire evidence should not require adding print statements to protocol code.

The runtime will emit bounded structured trace events containing endpoint role, direction, message class, service, invoke ID, associate ID, byte count and timing. Raw payload retention is separately bounded/configurable.

## Current compatibility evidence

The native codec currently round-trips captured FT20 BER messages for:

- Association response;
- GetServerDirectory response;
- GetLogicalDeviceDirectory response;
- GetLogicalNodeDirectory response.

The native in-memory `ServerCore` also produces byte-identical responses for those tested vectors from its own deterministic model.

This proves only the specific pinned laboratory profile covered by the vectors. It is not a general IEC 61850-8-3 conformance claim.

## Dependency boundary

The production runtime in this repository must never:

- import Netbeheer Python modules;
- spawn a Netbeheer process;
- download Netbeheer on startup;
- require Python to serve DMS;
- require the Netbeheer repository for normal operation.

External Netbeheer runs belong only in optional interoperability tests.
