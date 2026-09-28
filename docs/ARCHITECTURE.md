# Architecture

## Target shape

```text
Applications / Workbench / CI
          |
     Public C++ API
          |
+-----------------------------+
| Canonical IEC 61850 domain  |
| model + ACSI service model  |
+-----------------------------+
      |                 |
 MMS adapter         DMS adapter
 (ARStack)          (this repo)
                        |
              +-------------------+
              | Session / service |
              +-------------------+
                 |            |
              Codec         Trace
          BER/DER/JER      evidence
                 |
           IWireTransport
                 |
             WebSocket
```

The key invariant is that model, DataSet, RCB and report semantics are not owned by the WebSocket layer or by a specific encoding.

## Runtime

The runtime is intentionally split into bounded workers:

- **I/O worker**: socket reads/writes and framing;
- **decode worker**: wire bytes -> canonical PDU;
- **service worker**: association/request/report state machines;
- **observer path**: bounded trace/events for GUI and test tooling.

A slow GUI or trace consumer must not block the protocol service path.

## Client state

```text
Disconnected -> Connecting -> Associated -> Releasing -> Disconnected
                         \-> Failed
```

Each confirmed request owns an `invokeId`, timestamp, service identity and timeout budget. Outstanding requests are bounded.

## Reporting

Reporting is event driven. A report update enters the application as a `ReportEvent` carrying:

- report ID;
- DataSet reference;
- sequence number;
- configuration revision;
- reason-for-inclusion per member;
- values and, when present, quality/timestamp.

A client may choose to verify a report with an explicit read, but that is policy and never a hidden fallback.

## Compatibility strategy

The public Netbeheer implementation is treated as a reference endpoint for laboratory interoperability. We will pin exact upstream revisions and keep captured vectors so upstream changes cannot silently redefine our test baseline.
