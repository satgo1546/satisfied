@echo off
path %PATH%;%ANDROID_SDK_ROOT%\tools;%ANDROID_SDK_ROOT%\platform-tools;%ANDROID_SDK_ROOT%\build-tools\25.0.3
cmd
rem aapt d xmltree com.package.apk AndroidManifest.xml
