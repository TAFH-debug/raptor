# Raptor
All-in-one high performance protocol over TCP written in C++. 
Supports such features as RPC, event system, auto serialization/deserialization, error handling.
Successor of [netio](https://github.com/TAFH-debug/netio)

## Protocol Specification

### Handshake

The stream of bytes over TCP must start with "handshake". It consists out of these 6 bytes:
```
RAPTOR_HEADER - 0x23 0x41 0x32
RAPTOR_VERSION - 0x00 0x00 0x01
```

### Message format

```
// To Be Done
```
