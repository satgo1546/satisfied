@echo off
rem http://www.hanshq.net/command-line-android.html
if not exist build\apk\lib\armeabi-v7a mkdir build\apk\lib\armeabi-v7a
if not exist build\apk\lib\x86 mkdir build\apk\lib\x86

echo smali
::java -jar smali-2.5.2.jar assemble --api 16 --output build\apk\classes.dex MainActivity.smali
::java -jar smali-2.5.2.jar assemble --api 16 --output build\apk\classes.dex out\net\hanshq\hello\MainActivity.smali
type nul
if errorlevel 1 goto :EOF
echo arm-linux-androideabi-gcc
%ANDROID_SDK_ROOT%\android-ndk-r13b\toolchains\arm-linux-androideabi-4.9\prebuilt\windows-x86_64\bin\arm-linux-androideabi-gcc -Wall -Wextra -Wno-unused-parameter -std=gnu11 --sysroot=%ANDROID_SDK_ROOT%\android-ndk-r13b\platforms\android-9\arch-arm -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp -Wl,--fix-cortex-a8 -fPIC -shared -o build/apk/lib/armeabi-v7a/libsomelib.so somelib.c -landroid
if errorlevel 1 goto :EOF
echo i686-linux-android-gcc
%ANDROID_SDK_ROOT%\android-ndk-r13b\toolchains\x86-4.9\prebuilt\windows-x86_64\bin\i686-linux-android-gcc -Wall -Wextra -Wno-unused-parameter -std=gnu11 --sysroot=%ANDROID_SDK_ROOT%\android-ndk-r13b\platforms\android-9\arch-x86 -fPIC -shared -o build/apk/lib/x86/libsomelib.so somelib.c -landroid
echo aapt
%ANDROID_SDK_ROOT%\build-tools\25.0.3\aapt package -f -M AndroidManifest.xml -A assets -S res -I %ANDROID_SDK_ROOT%\platforms\android-16\android.jar -F build\Hello.unsigned.apk build\apk
echo zipalign
%ANDROID_SDK_ROOT%\build-tools\25.0.3\zipalign -f -p 4 build\Hello.unsigned.apk build\Hello.aligned.apk
echo apksigner
call %ANDROID_SDK_ROOT%\build-tools\25.0.3\apksigner sign --ks ..\sto\keystore.jks --ks-key-alias effandroid --ks-pass pass:wtf --key-pass pass:wtf --out build/Hello.apk build/Hello.aligned.apk
echo adb install
for /f "skip=1 tokens=1" %%i in ('%ANDROID_SDK_ROOT%\platform-tools\adb devices') do %ANDROID_SDK_ROOT%\platform-tools\adb -s %%i install -r build/Hello.apk
