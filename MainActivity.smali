.class public Lnet/hanshq/hello/MainActivity;
.super Landroid/app/Activity;

.method static constructor <clinit>()V
	.locals 1
	const-string v0, "somelib"
	invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
	return-void
.end method

.method public constructor <init>()V
	.locals 1
	invoke-direct {p0}, Landroid/app/Activity;-><init>()V
	return-void
.end method

.method public native getMessage()Ljava/lang/String;
.end method

.method protected onCreate(Landroid/os/Bundle;)V
	.locals 8
	invoke-super {p0, p1}, Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V

	new-instance v0, Ljava/lang/StringBuilder;
	invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V
	const-string v1, "你的 Android 平台 SDK 级别是 "
	invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
	move-result-object v0
	sget v1, Landroid/os/Build$VERSION;->SDK_INT:I
	invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;
	move-result-object v0
	const-string v1, "。"
	invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
	move-result-object v0
	invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
	move-result-object v0
	const v1, 0x0
	invoke-static {p0, v0, v1}, Landroid/widget/Toast;->makeText(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;
	move-result-object v0
	invoke-virtual {v0}, Landroid/widget/Toast;->show()V

	new-instance v0, Landroid/widget/RelativeLayout;
	invoke-direct {v0, p0}, Landroid/widget/RelativeLayout;-><init>(Landroid/content/Context;)V
	new-instance v1, Landroid/widget/RelativeLayout$LayoutParams;
	const v2, -0x1
	invoke-direct {v1, v2, v2}, Landroid/widget/RelativeLayout$LayoutParams;-><init>(II)V
	invoke-virtual {v0, v1}, Landroid/widget/RelativeLayout;->setLayoutParams(Landroid/view/ViewGroup$LayoutParams;)V
	new-instance v2, Landroid/widget/TextView;
	invoke-direct {v2, p0}, Landroid/widget/TextView;-><init>(Landroid/content/Context;)V
	new-instance v3, Landroid/widget/RelativeLayout$LayoutParams;
	const v4, -0x2
	invoke-direct {v3, v4, v4}, Landroid/widget/RelativeLayout$LayoutParams;-><init>(II)V
	new-instance v3, Landroid/widget/RelativeLayout$LayoutParams;
	const v4, 114
	const v5, 514
	invoke-direct {v3, v4, v5}, Landroid/widget/RelativeLayout$LayoutParams;-><init>(II)V
	invoke-virtual {v2, v3}, Landroid/widget/TextView;->setLayoutParams(Landroid/view/ViewGroup$LayoutParams;)V
	invoke-virtual {p0}, Lnet/hanshq/hello/MainActivity;->getMessage()Ljava/lang/String;
	move-result-object v4
	invoke-virtual {v2, v4}, Landroid/widget/TextView;->setText(Ljava/lang/CharSequence;)V
	const v4, 0xd
	invoke-virtual {v3, v4}, Landroid/widget/RelativeLayout$LayoutParams;->addRule(I)V
	invoke-virtual {v0, v2}, Landroid/widget/RelativeLayout;->addView(Landroid/view/View;)V
	new-instance v6, Landroid/widget/ImageView;
	invoke-direct {v6, p0}, Landroid/widget/ImageView;-><init>(Landroid/content/Context;)V
	const v7, 0x7f020000
	invoke-virtual {v6, v7}, Landroid/widget/ImageView;->setImageResource(I)V
	invoke-virtual {v0, v6}, Landroid/widget/RelativeLayout;->addView(Landroid/view/View;)V
	invoke-virtual {p0, v0, v1}, Lnet/hanshq/hello/MainActivity;->setContentView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V
	return-void
.end method
