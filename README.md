# Single-Producer / Single-Consumer Queue

A bounded, lock-free SPSC queue built as a fixed-capacity ring buffer
(`include/spsc_queue.hpp`). Capacity is chosen once at construction and the storage never
resizes, so there is no allocation on the hot path. The producer only ever writes the head
index, the consumer only ever writes the tail index, and each side reads the other's with
acquire/release ordering.

## Project layout

```
include/spsc_queue.hpp   the queue itself
src/main.cpp             demo -- one producer, one consumer, 100 items
tests/                   GoogleTest suite, basic + concurrent
bench/                   A/B benchmark with previous queue approach
```

## Assumptions / known limitations

One producer thread, one consumer thread. More of either is a data race, not just a lost item.

Nothing blocks either: `try_push` and `try_pop` return `false` when the queue is full
or empty, and the caller decides whether to spin, yield or give up (`src/main.cpp` spins).

Capacity is final once constructed, and the queue can't be copied or moved. Asking for 0
throws `std::invalid_argument`, since one slot is always held back and nothing would remain.

The ring is a plain `std::vector<T>` that builds every slot up front, so `T` needs a
default constructor. `bool` is refused outright by a `static_assert`: `std::vector<bool>`
packs bits, and both threads would end up writing the same word.

## Design notes

- One slot is always left unused, so `head == tail` means empty without a separate counter.
- The two indices sit on separate cache lines. Otherwise every push would invalidate the
  line the consumer is reading.
- Each side caches the other's index and only reloads the real atomic when that copy says
  full or empty.
- A failed `try_push` doesn't consume its argument, so a retry loop can move the same item
  again. That's what makes the queue usable with move-only types.
- `empty()` and `full()` are for logging only. They're racy, so use the `try_*` results.

## Build & run tests

```
cmake -S . -B build -DCMAKE_CXX_COMPILER=<path-to-compiler>
cmake --build build
ctest --test-dir build --output-on-failure
```

### Benchmark

Needs optimizations enabled, so build it Release:

```
cmake --build build --target spsc_bench --config Release
./build/spsc_bench
```

It compares the current queue against a frozen copy of the version from before the two cache
optimizations. Numbers are machine-specific; the gain measured 1.5x to 2.2x across runs here:

```
shared line, no cache     0.098 s   102.6 M ops/s    9.8 ns/op
padded + cached index     0.044 s   225.3 M ops/s    4.4 ns/op
```

## Tooling

CMake 3.14 or newer, C++17, and GoogleTest (fetched via `FetchContent`, no manual install).
