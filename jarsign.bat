keytool -genkeypair -keystore keystore.jks -alias a -validity 100000 -keyalg RSA -keysize 2048 -storepass 114514 -keypass 114514
::jarsigner -verify -verbose slzapk-output.apk
::apksigner verify --verbose slzapk-output.apk
jarsigner -keystore keystore.jks -storepass 114514 -keypass 114514 -sigalg SHA1withRSA -digestalg SHA1 slzapk-output.apk a
