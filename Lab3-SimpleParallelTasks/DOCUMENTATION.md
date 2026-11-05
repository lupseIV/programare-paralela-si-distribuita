# Lab 3 – Simple parallel tasks (matrix product)

## Problem

Compute `C = A · B` for square `n × n` matrices of 64-bit integers, splitting the elements of `C`
between `k` threads. The threads need no cooperation: each element is independent, every element is
written by exactly one thread, and `A`, `B` are read-only, so **no synchronization is needed**.
Joining the threads is the only synchronization point.

## Implementation

| Piece | Role |
|---|---|
| `src/Matrix.h` | Row-major matrix in a single contiguous buffer (`data[i*cols + j]`). |
| `computeElement(a, b, row, col, threadId)` | Computes one element: `Σ_k a(row,k)·b(k,col)`. In debug mode it prints `thread t -> (row, col)` (under a mutex so lines don't interleave). |
| `worker(a, b, result, strategy, threadId, k)` | **Thread function.** Calls `computeElement` for each element assigned to the thread. |
| `forEachAssigned(...)` | The work-splitting rules, shared by `worker` and the debug owner map. |
| `multiplyParallel` | Starts `k` threads running `worker`, joins them. |
| `multiplySequential` | i-k-j reference product, used to check every parallel result. |

### Splitting strategies

Elements are numbered `0 … n²-1`.

1. **rows** – row-major numbering; thread `t` takes the consecutive range
   `[t·⌊n²/k⌋, (t+1)·⌊n²/k⌋)`; the last thread also takes the remainder.
2. **columns** – same, but with column-major numbering (`row = idx % n`, `col = idx / n`).
3. **kth** – thread `t` takes `idx = t, t+k, t+2k, …` in row-major order.

Output of `Lab3_SimpleParallelTasks --debug 9 4` (owner map, thread id per cell) matches the examples
in the assignment exactly:

```
rows (20 20 20 21)      columns (20 20 20 21)   kth (21 20 20 20)
0 0 0 0 0 0 0 0 0       0 0 0 1 1 2 2 3 3       0 1 2 3 0 1 2 3 0
0 0 0 0 0 0 0 0 0       0 0 0 1 1 2 2 3 3       1 2 3 0 1 2 3 0 1
0 0 1 1 1 1 1 1 1       0 0 1 1 1 2 2 3 3       2 3 0 1 2 3 0 1 2
1 1 1 1 1 1 1 1 1       0 0 1 1 1 2 2 3 3       3 0 1 2 3 0 1 2 3
1 1 1 1 2 2 2 2 2       0 0 1 1 2 2 2 3 3       0 1 2 3 0 1 2 3 0
2 2 2 2 2 2 2 2 2       0 0 1 1 2 2 2 3 3       ...
2 2 2 2 2 2 3 3 3       0 0 1 1 2 2 3 3 3
3 3 3 3 3 3 3 3 3       0 0 1 1 2 2 3 3 3
3 3 3 3 3 3 3 3 3       0 0 1 1 2 2 3 3 3
```

The full trace (`thread 0 -> (0, 0)`, `thread 0 -> (0, 1)`, …) is printed before each map.

### Usage

```
Lab3_SimpleParallelTasks                       # benchmark, 3 runs per configuration -> results.csv
Lab3_SimpleParallelTasks --runs 5              # benchmark, 5 runs per configuration
Lab3_SimpleParallelTasks --debug [n] [threads] # trace + owner maps (default 9x9, 4 threads)
```

The debug printing is turned off during the benchmark: printing every element under a mutex would
serialize the threads and the timings would measure the console, not the computation.

## Experiments

**Machine:** AMD Ryzen 7 5800H (8 cores / 16 hardware threads, 512 KB L2 per core, 16 MB L3),
Windows 11, MinGW g++ (CLion bundled), Release build (`-O3`).
**Method:** for each size, thread count and strategy, 5 runs; times are wall-clock including thread
creation and join; the table shows the **minimum** in ms (averages are in `results.csv`). Every
result was compared with the sequential product: **all 144 configurations were correct**.

### Small matrices – thread overhead dominates

| n | strategy | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 256 |
|---|---|---|---|---|---|---|---|---|---|
| 10 | rows | **0.09** | 0.15 | 0.20 | 0.35 | 0.60 | 1.25 | 2.49 | 10.70 |
| 10 | kth | **0.09** | 0.15 | 0.23 | 0.41 | 0.72 | 1.36 | 2.60 | 11.36 |
| 50 | rows | **0.17** | 0.17 | 0.23 | 0.39 | 0.72 | 1.30 | 2.96 | 11.39 |
| 100 | rows | 0.76 | 0.58 | 0.52 | **0.45** | 0.78 | 1.55 | 3.59 | 14.95 |
| 100 | columns | 0.88 | 0.54 | **0.46** | 0.47 | 0.78 | 1.56 | 3.24 | 11.65 |
| 100 | kth | 0.73 | 0.44 | **0.41** | 0.51 | 0.66 | 1.47 | 2.89 | 22.92 |

### Large matrices – compute dominates

| n | strategy | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 256 |
|---|---|---|---|---|---|---|---|---|---|
| 250 | rows | 7.16 | 4.04 | 2.69 | 2.57 | **2.53** | 2.60 | 3.88 | 11.92 |
| 250 | columns | 7.07 | 3.82 | 2.20 | **1.53** | 2.13 | 2.37 | 3.40 | 11.01 |
| 250 | kth | 8.25 | 4.76 | **3.08** | 3.42 | 3.65 | 3.74 | 5.48 | 13.05 |
| 500 | rows | 75.0 | 39.5 | 28.2 | 19.9 | **17.9** | 19.1 | 19.8 | 25.0 |
| 500 | columns | 70.7 | 38.2 | 28.4 | **17.0** | 17.1 | 17.5 | 18.0 | 22.7 |
| 500 | kth | 76.3 | 45.8 | 30.5 | 33.1 | **28.6** | 32.0 | 30.7 | 35.0 |
| 1000 | rows | 649 | 341 | 235 | 162 | **143** | 145 | 143 | 148 |
| 1000 | columns | 634 | 336 | 226 | 155 | **134** | 141 | 136 | 141 |
| 1000 | kth | 710 | 417 | 317 | 258 | **233** | 241 | 238 | 239 |

Best speedups at n = 1000: rows 4.5×, columns 4.7×, kth 3.0× (relative to that strategy with one thread).

## Observations

### 1. Cost of creating threads
For n = 10 the whole product takes ~0.09 ms with one thread, and the time then grows **linearly with
the number of threads**: ≈ (10.7 − 0.09) / 255 ≈ **40 µs to create and join one thread** on this
system. With 256 threads, a 10 × 10 product is >100× slower than sequential. Threads only pay off when
each one gets enough work: here the break-even is around n ≈ 100 (1 000 000 multiply-adds), where 4–8
threads are best and 16+ are already slower again.

### 2. Number of threads vs. cores
For large matrices the time drops almost linearly up to 4 threads, then flattens: 8 threads (= the
number of physical cores) give 4×, 16 threads (= hardware threads, SMT) add only ~10–15 % more, and
32/64/256 threads give nothing. Extra threads just compete for the same cores, so the OS has to switch
between them. That is not visible as a slowdown at n = 1000 (≈ 0.15 s total) because 256 · 40 µs ≈
10 ms is small compared with the computation, but it is clearly visible at n ≤ 250.
Speedup stays below 8× because the naive algorithm is memory-bound: every element reads a column of B
with stride `n·8` bytes, so the cores wait on the shared L3/RAM.

### 3. Caching effects – rows vs. columns vs. kth
* **rows / columns** are close; columns is slightly faster (≈ 5 %). With *rows*, consecutive elements of
  a thread share the same row of A (contiguous, stays in L1) and walk across B's columns. With
  *columns*, consecutive elements share the same **column of B**: the strided column (n cache lines)
  stays hot in L1/L2 and is reused for n elements, while the rows of A are read contiguously, which the
  hardware prefetcher handles well. Each thread also works on its own contiguous block of C.
* **kth is clearly the slowest with many threads** (233 ms vs 134–143 ms at n = 1000, 16 threads),
  even though every thread does exactly the same number of multiplications. Two cache effects:
  - *Lost spatial locality in B:* one 64-byte cache line holds 8 consecutive elements `B(k, j…j+7)`.
    With *rows*, a thread computing columns j, j+1, … reuses the line it already loaded. With *kth* and
    k ≥ 8, a thread uses only every k-th column, so each line of B it loads serves just one of its
    elements and the other 7 values are wasted. That means up to 8× more memory traffic per thread.
  - *False sharing on C:* neighbouring elements of C (same cache line) are written by different
    threads, so that line keeps moving between cores. It matters less here (only n² writes against n³
    reads), but it adds to the cost.
  With one thread, all three strategies traverse C in nearly the same order and the times are within
  noise of each other (634–710 ms).

### Conclusions
* Splitting into **contiguous blocks** (rows or columns) is better than interleaving (kth): it keeps
  each thread's data in its own cache lines.
* The useful number of threads is about the number of hardware threads (8–16 here); above that there
  is no gain, only creation and switching overhead.
* For small problems, creating threads costs more than the work itself; the sequential version wins.
