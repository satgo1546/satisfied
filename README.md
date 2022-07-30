极度舒适
========

> 我就烂死！爽啦！

~~挖坑不填中~~

<dl>
	<dt>satisfied</dt>
	<dd>Windows，IA-32，在PATH中的NASM和Go。<a href="https://winworldpc.com/product/windows-nt-3x/31">目标是29年间的窗明几净</a>。顺带一提，这大概是第1546个带有lightweight话题的GitHub存储库。</dd>
	<dt>TRPG</dt>
	<dd>桌上复古编程游戏（Tabletop Retro-Programming Game）的简称，显然并没有人接受这一简称。跟多数桌上游戏不同，TRPG本质上并不是一种对战游戏，其根本目的是构造合适的指令序列，并在达成目的的同时优化代码的一个或多个方面。当代绝大多数TRPG规则都晦涩繁重，如主流的AMD64规则书分为五卷，总计超过三千页。</dd>
	<dt>something.asm</dt>
	<dd>几乎全手写的垃圾汇编，抄来的代码都作了标注。优化的目标既不是速度又不是大小，而是指令和宏的多样性。不过如果当真做到了极致的话很快就会受不了的吧。</dd>
	<dt>slzprog.go</dt>
	<dd>贼蠢的链接器，除了把PE头、代码、Win32资源贴在一起之外什么都不会，甚至不支持.rdata段。大部分代码是抄来的，也同样作了标注。</dd>
	<dt>gen.go</dt>
	<dd>静态单赋值形式的数据结构定义兼x86又臭又长代码生成后端。</dd>
	<dt>hilevel.go</dt>
	<dd>抽象语法树，同时负责编译到中间代码。这些编译器组件的详细信息参照<a href="https://satgo1546.github.io/satisfied/">网站</a>上的说明。</dd>
	<dt>run_test.go</dt>
	<dd>只有一项测试的“单元测试”，用来代替构建脚本。既然“测试一下”指的总是执行那些命令，输出的总是同一个结果，不如就套用上Go内置的测试解决方案吧。</dd>
	<dt>README.md</dt>
	<dd>Markdown的表现力不足以表达当前这个定义列表，它是用HTML语法插入的。行内代码不应被过度使用。中西文间不用加空格，（好，好，）<a href="https://chrome.google.com/webstore/detail/%E7%82%BA%E4%BB%80%E9%BA%BC%E4%BD%A0%E5%80%91%E5%B0%B1%E6%98%AF%E4%B8%8D%E8%83%BD%E5%8A%A0%E5%80%8B%E7%A9%BA%E6%A0%BC%E5%91%A2%EF%BC%9F/paphcfdffjnbcgkokihcdjliihicmbpd">那你装浏览器插件啊</a>。</dd>
</dl>

> It works! The screenshots below show the executable running on Windows NT 3.51 and Windows 10, versions that were released more than twenty years apart. For some reason, I find this extremely satisfying. (Perhaps in this sense, our executable is a lot more universal than programs using the newfangled Universal Windows Platform model?)
>
> — [*Othello for Desktop, Mobile and Web: an AI and GUI Exercise*](https://www.hanshq.net/othello.html)

> One of the reasons why I love working with a lot of these old technologies, is that I want any software work I'm involved in to stand the test of time with minimal toil. Similar to how the Super Mario Bros ROM has managed to survive all these years without needing a GitHub issue tracker.
>
> I believe the best chance we have of doing that, is by gluing together the binary interfaces that've already achieved a decades-long consensus, and ignoring the APIs. For example, here are the [magic numbers](https://github.com/jart/cosmopolitan/blob/1.0/libc/sysv/consts.sh) used by Mac, Linux, BSD, and Windows distros. They're worth seeing at least once in your life, since these numbers underpin the internals of nearly all the computers, servers, and phones you've used.
>
> If we focus on the subset of numbers all systems share in common, and compare it to their common ancestor, Bell System Five, we can see that few things about systems engineering have changed in the last 40 years at the binary level. Magnums are boring. Platforms can't break them without breaking themselves. Few people have proposed visions over the years on why UNIX numerology needs to change.
>
> — [*αcτµαlly pδrταblε εxεcµταblε*](https://justine.lol/ape.html)
