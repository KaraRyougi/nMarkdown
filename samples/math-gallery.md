# Math rendering gallery

This file exercises inline math, display layout, and the compact-screen edge cases most likely to expose bad vertical metrics.

Inline baseline: $a+b=c$, $x_i^2+y_j^2=r^2$, and $\frac{1}{2}+\sqrt{x}$ stay inside a normal line.

## Fractions and roots

$$\frac{a+b}{c+d}\qquad \frac{1+\frac{x}{y}}{1-\frac{x}{y}}$$

$$x=\frac{-b\pm\sqrt{b^2-4ac}}{2a}\qquad \sqrt[3]{\frac{x^2+1}{y_0}}$$

## Scripts and operators

$$x_i^2+x_{i_j}^{n+1}+e^{-x^2}\qquad a^{b^{c^d}}$$

$$\sum_{i=0}^{n}i^2\qquad\prod_{k=1}^{m}k\qquad\int_a^b f(x)\,dx$$

## Stretchy delimiters

$$\left(\frac{x+1}{x-1}\right)\quad\left[\frac{a}{b}\right]\quad\left\{\frac{p}{q}\right\}$$

$$\left\langle\frac{u}{v},\frac{x}{y}\right\rangle\qquad\left|\frac{x^2}{y_1}\right|$$

## Arrays and matrices

$$\begin{pmatrix}a&b\\c&d\end{pmatrix}\begin{pmatrix}x_1\\x_2\end{pmatrix}=\begin{pmatrix}y_1\\y_2\end{pmatrix}$$

$$\begin{bmatrix}1&0&0\\0&1&0\\0&0&1\end{bmatrix}\qquad\begin{cases}x+y=3\\2x-y=0\end{cases}$$

## Accents and styles

$$\hat{x}+\bar{y}+\vec{v}+\dot{p}+\ddot{q}+\underline{z}$$

$$\mathbf{F}=m\mathbf{a}\qquad\mathrm{sin}(x)+\mathit{velocity}$$

$$\mathbb{R}^2\to\mathbb{R}\qquad\mathcal{F}(x)=\int f(x)\,dx$$

## Alignment and recovery

$$\begin{aligned}f(x)&=x^2+2x+1\\&=(x+1)^2\end{aligned}$$

Malformed formulas remain local: $\frac{x}{$ and the following paragraph must still render.
