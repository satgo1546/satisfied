@echo off
echo zipalign
%ANDROID_SDK_ROOT%\build-tools\25.0.3\zipalign -f -p 4 build\Hello.unsigned.apk build\Hello.aligned.apk
echo apksigner
call %ANDROID_SDK_ROOT%\build-tools\25.0.3\apksigner sign --ks ..\sto\keystore.jks --ks-key-alias fuckandroid --ks-pass pass:fuckyou --key-pass pass:fuckyou --out build/Hello.apk build/Hello.aligned.apk
echo adb install
for /f "skip=1 tokens=1" %%i in ('%ANDROID_SDK_ROOT%\platform-tools\adb devices') do %ANDROID_SDK_ROOT%\platform-tools\adb -s %%i install -r build/Hello.apk
pause > nul
