#!/bin/sh
# Compile Tonton Launcher en APK signé, sans Gradle.
# Outils : paquets Ubuntu android-sdk-platform-23 aapt dalvik-exchange zipalign apksigner + un JDK.
set -e
cd "$(dirname "$0")"
SDK_JAR=/usr/lib/android-sdk/platforms/android-23/android.jar
OUT=build
rm -rf $OUT && mkdir -p $OUT/gen $OUT/classes

echo "1/5 Ressources"
aapt package -f -m -J $OUT/gen -M AndroidManifest.xml -S res -I $SDK_JAR

echo "2/5 Compilation Java"
javac -nowarn -source 8 -target 8 -encoding UTF-8 -Xlint:-options \
      -bootclasspath $SDK_JAR -d $OUT/classes \
      $(find src $OUT/gen -name '*.java')

echo "3/5 Conversion en code Android (dex)"
dalvik-exchange --dex --output=$OUT/classes.dex $OUT/classes

echo "4/5 Assemblage de l'APK"
aapt package -f -M AndroidManifest.xml -S res -I $SDK_JAR -F $OUT/app.unaligned.apk
(cd $OUT && aapt add -f app.unaligned.apk classes.dex >/dev/null)
zipalign -f 4 $OUT/app.unaligned.apk $OUT/app.aligned.apk

echo "5/5 Signature"
KEY=signing.keystore
if [ ! -f $KEY ]; then
  keytool -genkeypair -keystore $KEY -storepass tonton -keypass tonton -alias tonton \
          -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Tonton Launcher" >/dev/null 2>&1
fi
apksigner sign --ks $KEY --ks-pass pass:tonton --key-pass pass:tonton --ks-key-alias tonton \
          --out TontonLauncher.apk $OUT/app.aligned.apk
apksigner verify TontonLauncher.apk && echo "OK : TontonLauncher.apk"
