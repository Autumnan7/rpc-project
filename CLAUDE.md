# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
mkdir build && cd build
cmake ..
make

# Run smoke test
./bin/rpc_demo

# Build and run specific example
make && ./bin/rpc_demo
```

## Architecture Overview

This is a user-mode coroutine scheduling framework based on GNU ucontext API.

### Core Layers (bottom-up):

1. **Memory Layer** (`mempool.h`, `objpool.h`)
   - `MemPool<T>`: Fixed-size memory pool using free-list, grows incrementally
   - `ObjPool<T>`: Object pool with placement new, handles trivial/non-trivial types via `std::integral_constant` dispatch

2. **Context Layer** (`context.h`, `coroutine.h`)
   - `Context`: Wraps `ucontext_t`, manages stack allocation and context switching
   - `Coroutine`: Encapsulates coroutine state (READY/RUNNING/SUSPEND/DEAD), function, and context

3. **Scheduling Layer** (`processor.h`, `scheduler.h`, `processor_selector.h`)
   - `Processor`: Single-threaded scheduler with event loop (timer → pending → epoll → cleanup)
   - `Scheduler`: Global singleton managing multiple Processors, auto-selects by CPU core count
   - `ProcessorSelector`: Strategies include MIN_EVENT_FIRST (default) and ROUND_ROBIN

4. **Event Layer** (`epoller.h`, `timer.h`)
   - `Epoller`: EPOLLIN/EPOLLOUT event registration with coroutine mapping
   - `Timer`: Priority queue (min-heap) of (Time, Coroutine*) pairs, uses timerfd for wakeups

5. **Synchronization** (`mutex.h`, `spinlock.h`, `spinlock_guard.h`)
   - `Spinlock`: Atomic compare-exchange lock
   - `SpinlockGuard`: RAII wrapper
   - `RWMutex`: Coroutine-safe read-write lock with waiting queue

### Key Design Patterns:
- Double-buffered pending queue in Processor reduces lock contention
- Object pooling for Coroutine allocation
- Thread-local `threadIdx` for processor identification
- RAII for resource management throughout

### Public API (`minico_api.h`):
```cpp
void co_go(std::function<void()>, size_t stackSize, int tid = -1);
void co_sleep(Time);
void sche_join();
```

## Known Issues to Address

1. `include/context.h:1` - Circular self-include
2. `src/context.cpp:55` - Uses hardcoded stack size instead of constructor parameter (has TODO comment)
3. `src/processor.cpp:34-43` - Dead commented code in destructor
4. `include/mutex.h:23` - Recursive rlock() may cause issues in edge cases
5. `src/processor_selector.cpp:14` - `front()` access pattern is risky despite size check

## Testing

The primary test is `examples/processor_smoke_test.cpp` which validates:
- Coroutine creation and execution
- Timer-based suspension and resume
- Processor event loop functionality
