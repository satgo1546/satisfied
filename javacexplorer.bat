@echo off
if not exist build\gen mkdir build\gen
if not exist build\obj mkdir build\obj
echo aapt -J
%ANDROID_SDK_ROOT%\build-tools\25.0.3\aapt package -f -m -J build\gen -S res -M AndroidManifest.xml -I %ANDROID_SDK_ROOT%\platforms\android-16\android.jar
echo javac
Q:\Prgm\Java\jdk1.8.0_131\bin\javac -source 1.7 -target 1.7 -bootclasspath Q:\Prgm\Java\jdk1.8.0_131\jre\lib\rt.jar -classpath %ANDROID_SDK_ROOT%\platforms\android-16\android.jar -g:none -encoding utf8 -d build\obj build\gen\net\hanshq\hello\R.java java\net\hanshq\hello\MainActivity.java java\net\hanshq\hello\FileProvider.java
if errorlevel 1 goto :EOF
echo dx
call %ANDROID_SDK_ROOT%\build-tools\25.0.3\dx --dex --dump-to=classes.lst --verbose-dump --output=build\apk\classes.dex build\obj
echo baksmali
java -jar baksmali-2.5.2.jar disassemble --api 16 --use-locals --sequential-labels build\apk\classes.dex
