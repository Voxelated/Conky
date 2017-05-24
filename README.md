# Conky

Conky is a concurrency and parallelism library. It is designed to make writing
efficient multi-threaded and massively parallel code simple. The long term goal
is to allow automatic scalling across multiple multi-core machines, where each
machine may contain nodes with accelerators (GPUs, for example).

Initially the implementation is linux based, however, the intention is to provide cross-platform solution.

Currently, the main concurrent container is a circular work-stealing deque. The
dequeue allows the owning thread to push and pop onto the bottom of the dequeue,
and allows any other thread to steal from the top of the dequeue. The number of
elements which the dequeue may store is compile-time defined, and the implementation is further optimised for power of 2 sized elements, so power of 2 sizes should be used.

Built on top of the circular dequeue are thread pools -- both dynamically and statically sized. Both of the thread pools are built on top of the circular
work stealing dequeues. The thread pool stores a circular dequeue for each of the threads in the pool. The decision on which thread's queue to steal from is based on locality -- a thread should first try and steal from another thread in the same NUMA node, and then on the same machine, and then on the next closest (physically) machine. This should reduce latency, but may not result in optimal
load balancing. To achieve optimal load balancing is more complex, and requires more inter-thread communication, so would likely be slower. This may be added eventually.

## Dependencies

Conky requires the general purpose utility library from Voxel, which can be
found at: [[repository](https://github.com/Voxelated/Voxel)|
[documentation](https://voxelated.github.io/libraries/voxel/index.html)]

## Licensing

This is completely free software, you can do whatever you like with it.


