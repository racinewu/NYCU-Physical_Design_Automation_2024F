# Corner Stitching Data Structure
The Corner Stitching Lab implements a tile-based spatial data structure used in VLSI physical design to efficiently represent and query a 2D chip layout. The plane is partitioned into non-overlapping rectangular tiles of two types: solid block tiles (representing placed circuit components) and space tiles (representing free routing area). Space tiles are maintained as maximal horizontal stripes, meaning each space tile spans as far left and right as possible without being interrupted by a block tile.

## Problem Formulation
Given a rectangular outline of width W and height H with lower-left corner at the origin, the system processes two types of operations in input order: Block Creation, which inserts a rectangular block into the layout and restructures the surrounding space tiles accordingly, and Point Finding, which queries the tile occupying a given coordinate at the moment of the query. The goal is to maintain a valid corner-stitched tile plane and produce the following outputs: (1) the total number of tiles (block + space) in the final layout, (2) for each block tile in ascending index order, the count of distinct neighboring block tiles and neighboring space tiles sharing a non-zero-length edge, and (3) for each point query, the lower-left corner coordinate of the tile containing that point at the time the query is issued. A point on the right or top edge of a tile does not belong to that tile.

## Features
- **Half-Open Tiles with Corner Stitches**: tiles cover `[x1, x2) × [y1, y2)` with four stitches (`tr`/`rt`/`bl`/`lb`); right/top-edge exclusion encodes the spec's boundary rule, and stitch walks give near-O(1) point finding and neighbor counting.
- **Stitch-Maintaining Primitives**: `splitH`, `splitV`, and `mergeV` centralize every stitch update; higher-level operations compose these and never touch stitches directly.
- **Canonical Block Insertion**: cut along top/bottom edges -> sweep strips top-down -> split off left/right remainders -> chain centers via `mergeV` -> restore maximal-horizontal-strip form by merging same-span space.
- **Free-List Recycling**: merged-away tiles are marked dead and reused by `newTile()`; debug-mode asserts on primitive preconditions trap corrupt stitches at the first bad call.

## Processing Pipeline
1. **Parse**: read outline `W H` from line 1 and buffer all remaining commands as block insertions or point queries.
2. **Init**: allocate a single space tile covering `[0, W) × [0, H)` and set `hint` to it.
3. **Execute**: replay buffered commands in input order against the corner-stitching plane.
   1. **Point Query**: alternating vertical/horizontal walk from `hint`; record the lower-left corner at query time.
   2. **Block Insertion**: `splitH` along block edges, sweep strips with `splitV`, chain centers with `mergeV` and mark solid.
   3. **Canonical Maintenance**: vertically merge remainders with same-span space to preserve maximal-horizontal-strip form.
4. **Output**: write tile count, then `[idx] [block_neighbors] [space_neighbors]` per block in ascending index order, then point-query results in input order.


## Input / Output Format
### Input
- All input values are integers.
- All blocks are rectangular and do not overlap or exceed the outline.
- Block indices are positive, may be non-consecutive, and fall within the valid integer range.
- The Point_Finding coordinates range from 0 to outline width/height - 1 in X/Y.
- The total number of blocks ranges from 1 to 25,000.
- The outline width and height are each up to 200,000.

**Input.txt**
```
<outline_width> <outline_height>
{ P <X_pos> <Y_pos> | <blk_id> <lower_left_x> <lower_left_y> <width> <height> }
# repeated one or more times
...
```

**Example**
```
100 100
P 35 35
1 35 35 30 30
2 35 65 60 20
3 65 5 20 60
4 5 15 60 20
5 15 35 20 60
P 35 35
```

### Output
- For each point, output the block’s lower-left coordinate based on its **placement when read**, following the **P command order**.
- Each point belongs to a single block. Points on edges follow this rule: **right and top edges are excluded**.
- Blocks touching only at corners are not considered neighbors.
- Blocks may touch other blocks or the outline.

**Output.txt**
```
<num_tiles>
<blk_id> <num_neighbor_blks> <num_neighbor_spaces>
# repeat <num_blk> times, in ascending order by id 
...
<lower_left_x> <lower_left_y>
# repeat <num_P> times
...
```

**Example**
```
13
1 4 0
2 3 3
3 3 3
4 3 3
5 3 3
0 0
35 35
```

## Directory Structure
```
Lab1/
  ├── Makefile             // Build script to compile the project
  ├── testcase/
  │   ├── input
  │   └── golden
  │
  ├── include/
  │   └── stitcher.h
  │
  ├── src/
  │   ├── main.cpp         // Main entry point
  │   └── sticher.cpp      // Stitcher
  │
  ├── build/               // Object (.o) and dependency (.d) files created during build
  ├── bin/                 // Final executable, e.g., bin/CStitch
  ├── run.sh               // Shell script to manage all testcases
  │
  └── README.md
```
## Usage Guide
### How to compile
To generate the executable `bin/cstitch`, simply run
```
make
make VERBOSE=1 // Verbose logging
```
### How to execute
Run the program with 60s execution time limit.
```
timeout 60s ./bin/cstitch <input>.txt <output>.txt
```
### Utility Scripts
To manage testcases, use `run.sh`.
```
./run.sh <case|all> [check|clean|valgrind]
```
