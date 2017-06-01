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

Built on top of the circular dequeue is a thread pool, where the size of each
of the per thread task queues can be specified, as well as the alignment for
the tasks, and the stealing policy.

The currently implemented stealing policies are __random__
and __nearest neighbour__. Random randmly generates the index of the queue to
steal from, while nearest neighbour looks for the nearest neighbours, and looks
one neighbour further each time it fails to steal. Nearest neighbour generally
has better performance, and benchmarks show that the performance under
contention is far superior.

The tasks stores by the thread pool are completely unintrusive -- it is not
required that a class overrides the task type or anything like that, so
pretty much __anything__ can be pushed onto the thread pool, it just needs to
have a call operator, and if it required parameters, the values of the
parameters must be given when pushing the work onto the queue.

~~~cpp
static constexpr iterations = 100;

Voxx::System::CpuInfo::refresh();
Voxx::Conky::ThreadPool<1024> threadPool(Voxx::System::CpuInfo::cores());

template <typename T>
struct WorkFunctor {
  void operator(float u) {
    // Functor body ...
  };
  T t;
};

for (int i = 0; i < iterations; i++) {
  if (i % 2) {
    threadPool.tryPush(WorkFunctor<double>(), uValue);
    continue;
  }

  threadPool.tryPush([] (auto someValue, float someOtherType) {
    // Some other work body ...
  }, someValueToUse, someOtherTypeToUse);
}

threadPool.waitTillIdle();
~~~

The performance of the thread pool is shown to be very good by the provided
benchmarks (located in the ``tests`` directory).

## High Level Algorithms

## Dependencies

Conky requires the general purpose utility library from Voxel, which can be
found at: [[repository](https://github.com/Voxelated/Voxel)|
[documentation](https://voxelated.github.io/libraries/voxel/index.html)]

## Licensing

This is completely free software, you can do whatever you like with it.