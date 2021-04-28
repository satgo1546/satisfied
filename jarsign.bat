keytool -genkeypair -keystore keystore.jks -alias effandroid -validity 100000 -keyalg RSA -keysize 2048 -storepass wtf -keypass wtf
jarsigner -keystore keystore.jks -storepass wtf -keypass wtf sto.jar effandroid
