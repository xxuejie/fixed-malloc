# fixed-malloc

A memory allocator working within fixed memory region, used for embedded envoronments, such as Nervos CKB. See [here](./docs/design.md) for the overall design.

The very core of this new malloc is implemented in pure C99, Rust binding is also provided for integration into Rust projects. We also employ Rust extensively for testing purposes.
