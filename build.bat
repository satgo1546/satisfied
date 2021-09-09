@echo off
rem http://www.hanshq.net/command-line-android.html

mkdir "%TEMP%\build\obj"
echo javac
Q:\Prgm\Java\jdk1.8.0_131\bin\javac -source 1.7 -target 1.7 -bootclasspath Q:\Prgm\Java\jdk1.8.0_131\jre\lib\rt.jar -classpath %ANDROID_SDK_ROOT%\platforms\android-16\android.jar -g:none -encoding utf8 -d "%TEMP%\build\obj" MainActivity.java FileProvider.java
if errorlevel 1 goto :EOF
echo dx
call %ANDROID_SDK_ROOT%\build-tools\25.0.3\dx --dex --dump-to=classes.lst --verbose-dump --output=classes.dex "%TEMP%\build\obj"
::echo baksmali
::java -jar baksmali-2.5.2.jar disassemble --api 16 --use-locals --sequential-labels classes.dex
type nul
::echo smali
::java -jar smali-2.5.2.jar assemble --api 16 --output build\apk\classes.dex MainActivity.smali
::java -jar smali-2.5.2.jar assemble --api 16 --output build\apk\classes.dex out\net\hanshq\hello\MainActivity.smali
type nul
if errorlevel 1 goto :eof
echo arm-linux-androideabi-gcc
%ANDROID_SDK_ROOT%\android-ndk-r13b\toolchains\arm-linux-androideabi-4.9\prebuilt\windows-x86_64\bin\arm-linux-androideabi-gcc -Wall -Wextra -Wno-unused-parameter -std=gnu11 --sysroot=%ANDROID_SDK_ROOT%\android-ndk-r13b\platforms\android-9\arch-arm -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp -Wl,--fix-cortex-a8 -fPIC -shared -o arm.so somelib.c -landroid
if errorlevel 1 goto :eof
echo i686-linux-android-gcc
%ANDROID_SDK_ROOT%\android-ndk-r13b\toolchains\x86-4.9\prebuilt\windows-x86_64\bin\i686-linux-android-gcc -Wall -Wextra -Wno-unused-parameter -std=gnu11 --sysroot=%ANDROID_SDK_ROOT%\android-ndk-r13b\platforms\android-9\arch-x86 -fPIC -shared -o x86.so somelib.c -landroid
echo gcc ^&^& apk
gcc -Wall -Wextra -Wno-unused-parameter -Wimplicit-fallthrough=2 -std=gnu11 apk.c
if errorlevel 1 goto :eof
a
echo apksigner
rem If faced with an incompatible certificate error, do `adb shell pm uninstall com.example.hello` first. The useless "Settings" app provides no way to clean up an app completely.
call %ANDROID_SDK_ROOT%\build-tools\25.0.3\apksigner sign --ks keystore.jks --ks-key-alias a --ks-pass pass:114514 --key-pass pass:114514 --out Hello.apk slzapk-output.apk
echo adb install
for /f "skip=1 tokens=1" %%i in ('%ANDROID_SDK_ROOT%\platform-tools\adb devices') do %ANDROID_SDK_ROOT%\platform-tools\adb -s %%i install -r Hello.apk
