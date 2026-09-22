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
