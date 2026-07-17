# nMarkdown Reader

Real **Markdown**, Unicode, and anti-aliased text on a 320 × 240 RGB565 screen.

> A compact reader should still feel calm and legible. Ελληνικά · Привет · ∑ Ω

## Supported today

- [x] CommonMark blocks and inline styles
- [x] Tables, task lists, and ~~old text~~
- [ ] Mathematical layout is the next milestone

1. Scroll with the arrow keys.
2. Change text size with plus and minus.
3. Open the reader menu and switch themes.

Read the [project plan](../PLAN.md) for the complete architecture.

Inline math shares the text baseline: $ E = mc^2 $ and
$\frac{-b \pm \sqrt{b^2-4ac}}{2a}$.

$$\begin{bmatrix}a&b\\c&d\end{bmatrix}
\begin{pmatrix}x_1\\x_2\end{pmatrix}$$

```cpp
Surface565 page = display.surface();
render(page);
```

| Feature | Status |
|:--|--:|
| UTF-8 | ready |
| Lazy layout | ready |

---

Longer paragraphs wrap greedily at Unicode-safe boundaries. Offscreen blocks keep only their measured heights, while shaped line layouts are evicted by an LRU cache. This keeps scrolling responsive without laying out an entire book during startup.
