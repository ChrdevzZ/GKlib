# GKlib architecture notes

Agent workflow and editing contracts are in [AGENTS.md](../AGENTS.md). This page
keeps the useful architecture context from the former Claude-only guide.

GKlib uses C preprocessor templates for type-specific operations. The public
`include/gk_mk*.h` families implement BLAS-like operations, allocation, sorting,
priority queues, random operations and array/CSR helpers. Instances live in
`src/`; declarations in `include/gk_proto.h` use the owner's explicit export
attribute. Caller-defined instances must not accidentally import GKlib symbols.

`include/GKlib.h` is the public umbrella header. Generated configuration/export
headers accompany architecture definitions, types, structures, external state,
macros, getopt, template families and prototypes. Preserve those transitive
public headers; their names alone do not make them private implementation.

`gk_csr_t` and the CSR routines represent sparse matrices; `gk_graph_t` and graph
routines handle adjacency and weights. Typed key/value pairs and queues are
constructed through macro families. Existing names commonly use `gk_`, typed
operation suffixes and Create/Init/Free/Destroy lifecycle conventions. Follow
the relevant family rather than imposing a new naming pattern globally.

Error recovery uses setjmp/longjmp and configured thread-local state. Windows
shared builds expose error state through accessors; allocation tracking also
uses configured TLS. Assertion behavior is controlled by GKlib-specific policy
macros, with normal and expensive checks independent of consumer-wide NDEBUG.

Recovery bindings are synchronous: `gk_sigtrap` reserves a frame, then the
caller initializes it with `gk_sigcatch` before calling fallible code. Use
`gk_sigcatch` only in a C-standard `setjmp` expression context, such as the
controlling expression of a `switch`. `gk_errexit(SIGMEM/SIGERR, ...)` jumps to
the most recent binding when exit-on-error is enabled; returning mode reports
the error and returns. `gk_sigthrow` rethrows after local cleanup. Neither
modern nor legacy bindings install signal handlers. External signals, including
`raise(SIGMEM/SIGERR)`, remain the host's responsibility and do not trigger
GKlib recovery. The historical `gk_NonLocalExit_Handler` name is retained for
explicit synchronous legacy jumps, not asynchronous handler use.

On POSIX, each binding saves the calling thread's entry signal mask and restores
it on a jump or normal release using `pthread_sigmask`. A failed `gk_sigtrap`
returns zero with `errno` without publishing a new binding. A failed
`gk_siguntrap` returns zero with `errno` and keeps the original binding active
so the caller can retry while its original activation remains live. The legacy
`gk_UnsetSignalHandlers` interface is void and reports failure through `errno`;
failed mask restoration also keeps its established binding active. A caller
must not leave that activation after a failed release. Internal queue-template releases and
failed mask restoration during a jump use `_Exit(EXIT_FAILURE)` because a
valid recovered state cannot be guaranteed. This protocol failure bypasses
host signal and exit callbacks, C++ automatic cleanup and buffered output;
ordinary reported allocation or input errors retain their recovery policy.
Legacy bindings use the single public `gk_jbuf`, reject duplicate establishment
and must be released after their inner modern frames. All bindings are released in reverse order.
The public `jmp_buf` declarations and Windows DLL accessor layout are unchanged.
Windows recovery does not use signal masks. With TLS disabled, callers must
serialize recovery and allocation tracking. Even with TLS enabled, this protocol
does not make other global GKlib or METIS state fully concurrent.

The checked allocator, matrix, cache, mcore and hash-table paths validate sizes
before changing ownership. Returning failures and trapped allocation signals in
those paths leave previously committed state usable. The hardened graph, CSR,
sequence and I/O paths use `EINVAL` for invalid input, `EOVERFLOW` for an
unrepresentable allocation size and `ENOMEM` for allocation failure. I/O cleanup
retains the first system error or uses `EIO` when no lower-level error is
available.

`gk_getline` follows the POSIX buffer-ownership contract: an input buffer is null
or comes from libc `malloc`/`realloc`, and the returned buffer is released with
`free`. Sequence objects returned by `gk_seq_ReadGKMODPSSM` own their nested
arrays and are released with `gk_seq_free`. Graph and CSR objects store one
vertex-weight constraint; their METIS-format readers reject weighted inputs with
another `ncon` value rather than discarding constraints during later operations.

See [fork changes](../FORK_CHANGES.md) for the exact upstream differences and
[building](building.md) for CRT, runtime and package requirements.
