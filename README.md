## Compiling and Executing

compile with `gcc -g -o main main.c -lm`

run with `./main`

or run `./run.sh`

## Assumptions/Simplifications

L1 latency: .5ns

L2 latency: 5ns

DRAM latency: 50ns

L2 energy penalty: 5pJ

DRAM energy penalty: 640pJ

 - The above latencies are total latencies:
    - If a read from L2 has 5ns of latency, the L1 is active for .5ns and the L2 is active for 4.5ns.
    - A read from DRAM has total latency of 50ns; the L1 is active for .5ns, the L2 is active for 4.5ns, and DRAM is active for 45ns.
 - Writes to DRAM do not stall the processor and are only performed asynchronously on write back from L2.
    - Therefore, they do not contribute to the avg access time calculation.
    - They do contribute to DRAM active energy consumption.
 - Writes do not incur energy penalties:
    - An L2 hit on a write operation takes 5ns, but it does not incur the 5pJ penalty that an L2 hit on a read operation would. This is because we do not have to bring the line from L2 to L1 in a write -- we can write to L1 directly, whereas in a read, we would have to propogate the line from L2 to L1. 
    - The same applies to L2 misses on write operations. It takes 5ns because we can write directly to the lines in L2 and L1 instead of having to bring the data from DRAM. Thus, we do not incur the L2 penalty for the same reason stated above nor do we incur the DRAM penalty (because we avoid accessing DRAM entirely).
 - Evictions and write backs are accounted for in the latencies and penalties (they do not incur additional penalties). 
    - We do not distinguish between an L2 miss that does not result in a write back and an L2 miss that does in terms of latency and energy calculations for the caches. (Of course, the writeback will contribute to the DRAM's active energy consumption.)

## Implementation Details

L1-I and L1-D:
 - 32kB
 - Direct-mapped
 - Block size: 64B
 - Write-allocate
 - Write-through
 - Random replacement

L2 (unified):
 - 256kB
 - Set associativity of 2, 4, and 8
 - Block size: 64B
 - Write-allocate
 - Write-back
 - Random replacement
 - Inclusive of L1

### Write through and write back

This implementation uses write-through for L1 and write-back for L2. Because the L2 is inclusive of L1, L1 and L2 write hits are the same:
both situtations lead to writes to both L1 and L2.

DRAM is never written to directly. DRAM is only written to on write back when a dirty line is evicted from L2.