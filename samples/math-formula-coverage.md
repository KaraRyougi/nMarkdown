# Extended math formula coverage

This document exercises the TeX constructs used by `markdown-formula.md.tns`.
It is intentionally compact enough to copy to a calculator and inspect there.

## Inline symbols

Greek: $\alpha+\beta+\zeta+\eta+\iota+\kappa+\lambda+\mu+\nu+\xi+\omicron+\pi+\rho+\sigma+\tau+\upsilon+\phi+\varphi+\chi+\psi+\omega$.

Relations and sets: $A\notin B$, $x\ngeq y$, $A\subseteq B$, $A\cup B$, $A\cap B$, $A\setminus B$, $p\implies q$, and $p\iff q$.

Operators: $\iint_D f\,dx\,dy$, $\iiint_V f\,dV$, $\oint_C f\,ds$, $\bigoplus_i A_i$, and $\bigotimes_i B_i$.

## Delimiters and annotations

$$
\left\lfloor\frac{a}{b}\right\rfloor+
\left\lceil\frac{c}{d}\right\rceil+
\left\langle\frac{x}{y}\right\rangle
$$

$$
\overleftarrow{a+b+c}+\overrightarrow{x+y+z}+
\underleftrightarrow{u+v+w}
$$

$$
\overbrace{a+\underbrace{b+c}_{1.0}}^{2.0}
$$

## Calculus and continued fractions

$$
\lim_{x\to0}\frac{\sin x}{x}=1,
\qquad \iint_D f\,dx\,dy=\sigma,
\qquad \iiint_V f\,dV=\nu
$$

$$
x=a_0+\cfrac{1}{a_1+\cfrac{2^2}{a_2+\cfrac{3^2}{a_3+\cdots}}}
$$

## Cases, matrices, and determinants

$$
f(n)=\begin{cases}
n/2,&\text{if $n$ is even}\\
3n+1,&\text{if $n$ is odd}
\end{cases}
$$

$$
\begin{Bmatrix}1&2\\3&4\end{Bmatrix}
\quad
\begin{vmatrix}a&b\\c&d\end{vmatrix}
\quad
\begin{Vmatrix}x&y\\z&w\end{Vmatrix}
$$

## Arrays, alignment, and tags

$$
\begin{array}{c|lcr}
n&\text{左对齐}&\text{居中对齐}&\text{右对齐}\\
\hline
1&0.24&1&125\\
2&-1&189&-8
\end{array}
$$

$$
\begin{align}
v+w&=0&\text{Given}\tag 1\\
-w&=-w+0&\text{additive identity}\tag 2
\end{align}
$$

$$
f\left(
\left[
\frac{1+\left\{x,y\right\}}
{\left(\frac{x}{y}+\frac{y}{x}\right)(u+1)}+a
\right]^{3/2}
\right)\tag{公式1}
$$

## Legacy font declarations

$$
\rm D+\cal D+\it D+\Bbb D+\bf D+\mit D+
\sf D+\scr D+\tt D+\frak D+\boldsymbol D
$$
