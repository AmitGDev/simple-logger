# DFS & BFS

A small C++23 demo of binary tree traversal algorithms.

## What it shows

| Traversal | Variant | Data structure |
|---|---|---|
| DFS preorder | Recursive (`DfsR`) | Call stack |
| DFS postorder | Recursive (`DfsR`) | Call stack |
| DFS preorder | Iterative (`Dfs`) | `std::stack` |
| DFS postorder | Iterative (`Dfs`) | `std::stack` |
| BFS (level order) | Iterative (`Bfs`) | `std::queue` |

The demo builds this tree and prints the visit order of each traversal:

```text
        1
       / \
      2   4
     /     \
    3       5
```

## Expected output

```text
DfsR (preorder):  [1, 2, 3, 4, 5]
DfsR (postorder): [3, 2, 5, 4, 1]
Dfs (preorder):   [1, 2, 3, 4, 5]
Dfs (postorder):  [3, 2, 5, 4, 1]
Bfs:              [1, 2, 4, 3, 5]
```

The recursive and iterative DFS variants produce identical results for the same traversal order.

## Notes

- Iterative postorder pushes children left then right and prepends each visited node to the output. The final list is the reverse of a root-right-left walk, yielding left-right-root order.
- Preorder and postorder describe DFS visit order. BFS is level order and has no preorder/postorder equivalent.
- All traversals take O(n) time, where n is the number of nodes.
- Auxiliary space depends on the tree shape and traversal: recursive DFS uses O(h) call-stack space, iterative preorder uses O(h), iterative postorder may use O(n), and BFS uses O(w), where h is the tree height and w is the maximum tree width.

## Requirements

A C++23-compatible compiler and standard library. On Linux, GCC 16 or later.

## Build and Run

- [Build on Windows](https://github.com/AmitGDev/cpp-ci-template#build-on-windows)
- [Build on Linux](https://github.com/AmitGDev/cpp-ci-template#build-on-linux)

## Code Style

- [C++](https://github.com/AmitGDev/cpp-coding-standard)
- [Python](https://github.com/AmitGDev/cpp-ci-template#python)
- [CMake](https://github.com/AmitGDev/cpp-ci-template#cmake)

## License

MIT License. Copyright © 2024-2026 Amit Gefen.

---

**Last Updated:** 2026-10-22  
**Maintainer:** AmitGDev