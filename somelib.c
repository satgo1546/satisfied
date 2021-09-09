#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "bits.c"

static const char *const messages[] = {
	"Hello, world!",
	"你好，世界！",
	"草",
	"へん、へん、あああああ",
	"哼，哼，啊啊啊啊啊"
};

// https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/design.html
// The rules for C function names are:
// Java_⟨escaped fully-qualified class name⟩_⟨escaped method name⟩[__⟨escaped argument signature⟩]
// _0abcd = U+ABCD (lowercase only)
// _1 = the character “_”
// _2 = the character “;” in signatures
// _3 = the character “[“ in signatures

JNIEXPORT jstring JNICALL Java_net_hanshq_hello_MainActivity_getMessage(JNIEnv *env, jobject obj) {
	int i = rand() % (sizeof(messages) / sizeof(messages[0]));
	return (*env)->NewStringUTF(env, messages[i]);
}

// http://www.50ply.com/blog/2013/01/19/loading-compressed-android-assets-with-file-pointer/

static int fassetread(void *cookie, char *buf, int size) {
	return AAsset_read(cookie, buf, size);
}

static int fassetwrite(void *cookie, const char *buf, int size) {
	errno = EACCES;
	return -1;
}

static fpos_t fassetseek(void *cookie, fpos_t offset, int whence) {
	return AAsset_seek(cookie, offset, whence);
}

static int fassetclose(void *cookie) {
	AAsset_close(cookie);
	return 0;
}

static AAssetManager *asset_manager;
static jobject jasset_manager;

JNIEXPORT jint JNICALL Java_net_hanshq_hello_MainActivity_setAssetManager(JNIEnv *env, jobject this, jobject am) {
	jasset_manager = (*env)->NewGlobalRef(env, am);
	asset_manager = AAssetManager_fromJava(env, jasset_manager);
	return 42;
}

JNIEXPORT void JNICALL Java_net_hanshq_hello_MainActivity_disposeAssetManager(JNIEnv *env, jobject this) {
	(*env)->DeleteGlobalRef(env, jasset_manager);
	asset_manager = jasset_manager = NULL;
}

JNIEXPORT void JNICALL Java_net_hanshq_hello_MainActivity_nativeCall(JNIEnv *env, jobject this, jobject obj) {
  jclass cls = (*env)->GetObjectClass(env, obj);
  jmethodID mid_callback        = (*env)->GetMethodID      (env, cls, "callback"       , "()V");
  (*env)->CallVoidMethod(env, obj, mid_callback);
  jmethodID mid_callback_static = (*env)->GetStaticMethodID(env, cls, "callback_static", "()V");
  (*env)->CallStaticVoidMethod(env, cls, mid_callback_static);
}

FILE *funopen(const void *cookie, int (*readfn)(void *, char *, int), int (*writefn)(void *, const char *, int), fpos_t (*seekfn)(void *, fpos_t, int), int (*closefn)(void *));

FILE *fassetopen(const char *filename, const char* mode) {
	if (*mode == 'w') return errno = EACCES, NULL;
	AAsset* asset = AAssetManager_open(asset_manager, filename, 0);
	if (!asset) return NULL;
	return funopen(asset, fassetread, fassetwrite, fassetseek, fassetclose);
}

JNIEXPORT jint JNICALL Java_net_hanshq_hello_MainActivity_apkTest(JNIEnv *env, jobject obj, jstring jfilename) {
	const char *filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
	errno = 0;
	FILE *fp = fopen(filename, "wb");
	if (!fp) return errno;
	FILE *fin = fassetopen("small.apk", "rb");
	copy_file_fp(fp, fin);
	fclose(fin);
	fclose(fp);
	(*env)->ReleaseStringUTFChars(env, jfilename, filename);
	return errno;
}
