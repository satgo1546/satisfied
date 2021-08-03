PCBAD和MetaPath
===============

<dl>
	<dt>pcbad.c</dt>
	<dd>输出Gerber光绘和<a href="https://www.ucamco.com/files/downloads/file_en/305/xnc-format-specification_en.pdf">XNC钻孔</a>文件。Gerber X2和X3？做到.FileFunction就停下吧。加再多辅助属性，也不如补偿光栅化产生的伪像来得实在。</dd>
	<dt>mp.c</dt>
	<dd>用和METAFONT一样的方法求解路径控制点。代码是从MetaPost 1.504版一点点抄、改过来的。MetaPost于1.100版前后经历了<a href="https://www.tug.org/TUGboat/tb29-3/tb93hoekwater.pdf">一次大改</a>，易Pascal为C。自1.750版以降，<a href="https://www.tug.org/TUGboat/tb32-2/tb101hoekwater.pdf">MetaPost允许启动时改变数值计算引擎</a>，使C代码退化到如汇编般冗长不堪，故此处不再用更新的版本。</dd>
	<dt><ruby>其他<rt>C++</rt></ruby>语言做得到吗？</dt>
	<dd><code>void *</code>隐式转换（C89），结构体、联合体、枚举共用的命名空间（C89），匿名结构体（C11），按字段名初始化（C99），变长数组（C99），内建复数类型（C99）——these are a few of my favorite things…</dd>
</dl>
