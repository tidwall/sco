# sco

Coroutine scheduler for C.

The main purpose of this project is to power
[neco](https://github.com/tidwall/neco), which is a framework for building 
coroutine-based networking programs, but it works just as well for any program
that needs a fast and predictable scheduler.

This library uses [llco](https://github.com/tidwall/llco) for performing the
actual coroutine context switching.

## Features

- Fair and deterministic scheduler
- Stackful coroutines
- No allocations (bring your own stack)
- Cross-platform. Linux, Mac, Webassembly, FreeBSD, iOS, Android, Windows, RaspPi, RISC-V
- Fast context switching. Uses assembly in most cases
- Single file amalgamation. No dependencies
- Tests with 100% coverage

The coroutine switching ability is provided by [llco](https://github.com/tidwall/llco).

## API

```C
// Starts a new coroutine with the provided description.
void sco_start(struct sco_desc *desc);

// Causes the calling coroutine to relinquish the CPU.
// This operation should be called from a coroutine, otherwise it does nothing.
void sco_yield(void);

// Get the identifier for the current coroutine.
// This operation should be called from a coroutine, otherwise it returns zero.
int64_t sco_id(void);

// Pause the current coroutine.
// This operation should be called from a coroutine, otherwise it does nothing.
void sco_pause(void);

// Resume a paused coroutine.
// If the id is invalid or does not belong to a paused coroutine then this
// operation does nothing.
// Calling sco_resume(0) is a special case that continues a runloop. See the
// README for an example.
void sco_resume(int64_t id);

// Returns true if there are any coroutines running, yielding, or paused.
bool sco_active(void);

// Detach a coroutine from a thread.
// This allows for moving coroutines between threads.
// The coroutine must be currently paused before it can be detached, thus this
// operation cannot be called from the coroutine belonging to the provided id.
// If the id is invalid or does not belong to a paused coroutine then this
// operation does nothing.
void sco_detach(int64_t id);

// Attach a coroutine to a thread.
// This allows for moving coroutines between threads.
// If the id is invalid or does not belong to a detached coroutine then this
// operation does nothing.
// Once attached, the coroutine will be paused.
void sco_attach(int64_t id);

// Exit a coroutine early.
// This _will not_ exit the program. Rather, it's for ending the current 
// coroutine and quickly switching to the thread's runloop before any other
// scheduled (yielded) coroutines run.
// This operation should be called from a coroutine, otherwise it does nothing.
void sco_exit(void);

// Returns the user data of the currently running coroutine.
void *sco_udata(void);

// General information and statistics
size_t sco_info_scheduled(void);
size_t sco_info_running(void);
size_t sco_info_paused(void);
size_t sco_info_detached(void);
const char *sco_info_method(void);
```

## Example

A basic scheduler runloop.

```C
#include <stdlib.h>
#include <stdio.h>
#include "sco.h"

void entry(void *udata) {
    printf("Coroutine started\n");
    // The coroutine was started. Now we can do some processing. Or, start
    // another coroutine with sco_start(), pause this coroutine with 
    // sco_pause(), or exit prematurely with sco_exit().
}

void cleanup(void *stack, size_t stack_size, void *udata) {
    // The cleanup callback allows for freeing or recycling the stack belonging
    // to a recently ended coroutine. The current context of this function will 
    // _never_ be the same as the coroutine that is being cleaned up, so it's
    // best to avoid calling any sco_*() functions here.
    free(stack);
}

int main(void) {
    printf("Main thread\n");

    // Every coroutine has a stack, an entry function, a cleanup function,
    // and optional user data.
    struct sco_desc desc = {
        .stack = malloc(1048576), // 1 MB stack
        .stack_size = 1048576,
        .entry = entry,
        .cleanup = cleanup,
        .udata = NULL,
    };

    // Starting a coroutine from the main thread will block until all running
    // coroutines have completed, or until sco_pause() or sco_exit() is called.
    sco_start(&desc);
    
    // This is a basic scheduler runloop. It checks if there are any active 
    // coroutines that are paused. For paused coroutines, the loop can be a way
    // to handle event polling, sleeping, or other other mechanism that might
    // need to, at some later point, resume the coroutine.
    while (sco_active()) {
        // This point in the code provides some space for handling paused
        // coroutines. Or, in the case that sco_exit() was called, we can now
        // to choose to exit the runloop early or resume running coroutines.
        // The following sco_resume(0) will continue running scheduled
        // coroutines.
        sco_resume(0);
    }

    printf("All coroutines are done\n");
    return 0;
}
```

## Multiple threads

It's possible to have multiple threads, each running its own schdeduler.

```C
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h>
#include "sco.h"

void entry(void *udata) {
    printf("Started coroutine %" PRIi64 "\n", sco_id());
}

void cleanup(void *stack, size_t stack_size, void *udata) {
    free(stack);
}

void *thread(void *arg) {
    // Here's a distinct scheduler runloop that is independent from the other
    // threads. Ideally each thread starts an initial coroutine that does the 
    // main processing. 
    // From there you can move coroutines between threads using sco_detach()
    // and sco_attach().
    struct sco_desc desc = { 
        .stack = malloc(1048576), // 1 MB stack
        .stack_size = 1048576,
        .entry = entry,
        .cleanup = cleanup,
        .udata = NULL,
    };
    sco_start(&desc);
    while (sco_active()) {
        // .. handle paused coroutines here
        sco_resume(0);
    }
    return NULL;
}

int main(void) {
    pthread_t th1, th2, th3;

    // Start three threads that will each have their own scheduler.
    pthread_create(&th1, 0, thread, 0);
    pthread_create(&th2, 0, thread, 0);
    pthread_create(&th3, 0, thread, 0);

    // Wait for all threads to complete.
    pthread_join(th1, 0);
    pthread_join(th2, 0);
    pthread_join(th3, 0);

    return 0;
}
```

## Tests

Tests can be run from the project's root directory.

```bash
tests/run.sh
```

This will run all tests using the system's default compiler.

If [Clang](https://clang.llvm.org) is your compiler then you will also be 
provided with memory address sanitizing and code coverage.

### Examples

```bash
tests/run.sh                   # defaults
CC=clang-17 tests/run.sh       # use alternative compiler
CC=emcc tests/run.sh           # Test with emscripten
CFLAGS="-O3" tests/run.sh      # use custom cflags

