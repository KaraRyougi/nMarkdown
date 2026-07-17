# Typography and layout gallery

Plain body text, **strong text**, *italic text*, ***strong italic***,
~~strikethrough~~, and an [internal link](#cjk-and-mixed-scripts).

Punctuation and wrapping: “quoted text”, em—dash, ellipsis… and a final word.

## CJK and mixed scripts

中文排版测试：标点、换行与等宽混排。日本語かなカナ：句読点を確認。

English 与中文 mixed together; ASCII 123 and 日本語 remain on one baseline.

## Code and monospace

Inline code keeps cells even: `for (int i = 0; i < 3; ++i)`.

```cpp
const int width = 320;
const int height = 240;
draw_text("中文 / 日本語", width - 2);
```

## Formula coverage

Inline roots: $a + \sqrt{x^2+1} = b$ and $\sqrt[3]{u+v}$ stay on the text baseline.

$$\sqrt{\frac{x^2+y^2}{1+\sqrt{z+1}}} +
\frac{-b\pm\sqrt{b^2-4ac}}{2a}$$

$$A\begin{pmatrix}x_1\\x_2\end{pmatrix}=
\begin{bmatrix}a&b\\c&d\end{bmatrix}
\begin{pmatrix}x_1\\x_2\end{pmatrix}$$

## Blocks and alignment

> A quoted paragraph checks indentation, wrapping, and automatic line spacing.

- A list item with **strong text**
- A list item with CJK: 中文、日本語
- A list item with inline math: $\sum_{i=0}^{n}x_i^2$

| Format | Expected behavior |
| --- | --- |
| Text | Natural shaping and wrapping |
| CJK | Font fallback with stable spacing |
| Code | Monospace cells |
| Math | Connected radicals and scalable delimiters |
