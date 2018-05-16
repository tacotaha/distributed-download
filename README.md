# distributed-download
Bypass bandwidth restrictions by simultaneously transmitting partitions of a file across k clients, then combining the partitions into the original file upon completion.

```
     +------------------------+
     |                        |
     |    FILE (n bytes)      |
     |                        |
     |                        |
     ++--------+------------+-+
      |        |            |
      |        |            |
      |        |            |
      |        |            |
      v        v            v
+-----+--+  +--+-----+     ++-------+
|   n/k  |  |   n/k  | ... |   n/k  |
|  bytes |  |  bytes |     |  bytes |
+-----+--+  +--+-----+     ++-------+
      |        |            |
      |        |            |
      |        |            |
      V        V            V
     ++--------+------------+-+
     |                        |
     |    FILE (n bytes)      |
     |                        |
     |                        |
     +------------------------+
```
