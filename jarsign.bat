keytool -genkeypair -keystore keystore.jks -alias a -validity 100000 -keyalg RSA -keysize 2048 -storepass 114514 -keypass 114514
jarsigner -keystore keystore.jks -storepass 114514 -keypass 114514 -digestalg SHA-1 slzapk-output.apk a
