# Conky

Conky is a concurrency and parallelism library. It is designed to make writing
efficient multi-threaded and massively parallel code simple.

Conky uses a work-stealing queue for multi-threaded tasks, and cuda to
execute massively parallel tasks, if the system supports cuda. If cuda is not
supported then Conky will default to run jobs on the CPU. Conky always
creates __N__ threads for the CPU, where __N__ is the number of physical cores
in the system. The thread pool will not use hyperthreading (unless we test this
and it proves to be beneficial).


