# Transaction rollback

## What exists

Transactions are atomic. A throwing `Document::transact()` callable rolls back its writes and
rethrows. A `Transaction` destroyed before commit also rolls back. `Transaction::rollback()` is
public, idempotent, and prevents the transaction from committing.

## Design

The implementation applies local writes eagerly so transaction reads retain their existing
read-your-writes behavior. Each mutator records a compact inverse before it changes `DocState`:

- map mutations retain the previous key entries;
- list and text mutations retain inserted element IDs, previous element values, or visibility;
- marks and newly created objects retain their IDs for removal;
- commit retains the prior sequence number, heads, actor clock entry, and change-history size.

Rollback applies these records in reverse dependency order, restores `next_counter`, clears
pending operations, and marks the transaction inactive. Commit marks the transaction inactive
only after it has appended the change, so an exception while constructing a change can still be
rolled back.

## Why this design

Staging a copy of document state would make reads require an overlay and would copy growing
documents for ordinary transactions. The inverse journal keeps eager reads and allocates only
for data touched by the transaction.

## Performance

The benchmark command below compared the relevant transaction paths before and after this
change on an Apple M3 Max release build. Five repetitions with a 0.5-second minimum produced
these median CPU times:

| Benchmark | Before | After | Difference |
|---|---:|---:|---:|
| `bm_map_put` | 1,036 ns | 1,023 ns | -1.3% |
| `bm_list_insert_append` | 67,251 ns | 66,429 ns | -1.2% |
| `bm_text_splice_bulk` | 3,741,321 ns | 3,712,561 ns | -0.8% |

These differences are within the benchmark runs' observed variation; the journal introduces no
measurable committed-transaction regression in these paths.

```bash
./build-release/benchmarks/automerge_cpp_benchmarks \
    --benchmark_filter='^(bm_map_put|bm_list_insert_append|bm_text_splice_bulk)$' \
    --benchmark_min_time=0.5s \
    --benchmark_repetitions=5 \
    --benchmark_report_aggregates_only=true
```

## Verify and change safely

Run the full test suite and the benchmark command above. The regression tests cover a throwing
transaction across map, list, text, counter, mark, and nested-object writes; explicit rollback;
read-your-writes; and fixed committed bytes. When adding a mutator, add its inverse record before
the eager `DocState` write and extend the throwing-transaction test.
