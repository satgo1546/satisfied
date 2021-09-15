@echo off
path %PATH%;%ANDROID_SDK_ROOT%\tools;%ANDROID_SDK_ROOT%\platform-tools;%ANDROID_SDK_ROOT%\build-tools\25.0.3;Q:\Prgm\Java\jdk1.8.0_131\bin
cmd
rem aapt d xmltree com.package.apk AndroidManifest.xml
