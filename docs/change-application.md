# Change application

`Document::apply_changes()` is the checked entry point for externally supplied
changes. It returns `std::expected<void, Error>` and leaves the document
unchanged when validation fails.

## Deduplication and dependency ordering

Changes are identified by `ChangeHash`. A change already applied or already
waiting is ignored. A change whose dependencies are unavailable is retained in
the document's pending queue. Whenever changes arrive, the document applies
every queued change whose dependencies are now present, ensuring each
dependency is applied before its dependent.

`Document::get_missing_deps(heads)` returns the unavailable hashes required by
both the pending queue and `heads`. Hashes for changes already waiting locally
are omitted, so callers can request only changes that have not arrived.

`Document::merge()` and `Document::receive_sync_message()` both use this same
admission path. Re-sent changes therefore do not duplicate operations or
history entries.

## Validation

Before modifying the document, `apply_changes()` validates each new change:

- sequence numbers and operation counters start at one;
- a change contains operations with consecutive counters owned by its actor;
- operation kinds are recognized; and
- an actor and sequence number already present in history or the pending queue
  cannot identify a different change.

Invalid input returns `ErrorKind::invalid_change` with a reason. The entire
call is atomic: neither the visible state, heads, history, nor pending queue is
updated on validation failure.

## Verification

Run the complete test suite:

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

`tests/document_test.cpp` covers duplicate application, two- and three-change
out-of-order delivery, merge and sync re-delivery, missing-dependency reporting,
and rejection of malformed changes and conflicting actor sequences.
