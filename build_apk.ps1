$ErrorActionPreference = "Stop"
$SDK = "C:\Users\Dell\AppData\Local\Android\Sdk"
$BUILD_TOOLS = "$SDK\build-tools\35.0.0"
$PLATFORM = "$SDK\platforms\android-35\android.jar"
$JAVA_HOME = "C:\Program Files\Microsoft\jdk-17.0.21.21-hotspot"
$env:JAVA_HOME = $JAVA_HOME

Write-Host "=== 1. Compiling resources ==="
New-Item -ItemType Directory -Path "build\apk_classes" -Force | Out-Null
New-Item -ItemType Directory -Path "build\apk_dex" -Force | Out-Null
New-Item -ItemType Directory -Path "build\gen" -Force | Out-Null
New-Item -ItemType Directory -Path "build\apk" -Force | Out-Null

& "$BUILD_TOOLS\aapt2.exe" compile --dir apps\android\app\src\main\res -o build\compiled_res.zip

Write-Host "=== 2. Linking APK with modern SDK targets (minSdk=26, targetSdk=34) ==="
& "$BUILD_TOOLS\aapt2.exe" link `
    -I "$PLATFORM" `
    build\compiled_res.zip `
    --manifest apps\android\app\src\main\AndroidManifest.xml `
    --min-sdk-version 26 `
    --target-sdk-version 34 `
    --version-code 1 `
    --version-name "0.1.0" `
    --java build\gen `
    -o build\apk_base.apk `
    --auto-add-overlay

Write-Host "=== 3. Compiling Java sources ==="
Get-ChildItem "build\apk_classes" -Recurse | Remove-Item -Force -Recurse -ErrorAction SilentlyContinue
& "$JAVA_HOME\bin\javac.exe" `
    -source 17 -target 17 `
    -cp "$PLATFORM" `
    -d "build\apk_classes" `
    build\gen\org\openroute\client\R.java `
    apps\android\app\src\main\java\org\openroute\client\*.java

Write-Host "=== 4. Dexing classes with D8 (min-api 26) ==="
Get-ChildItem "build\apk_dex" -Recurse | Remove-Item -Force -Recurse -ErrorAction SilentlyContinue
$classes = Get-ChildItem "build\apk_classes\org\openroute\client" -Filter "*.class" | ForEach-Object { $_.FullName }
& "$BUILD_TOOLS\d8.bat" `
    --min-api 26 `
    --lib "$PLATFORM" `
    --output "build\apk_dex" `
    $classes

Write-Host "=== 5. Packaging classes.dex into APK ==="
& "$JAVA_HOME\bin\jar.exe" -uf build\apk_base.apk -C build\apk_dex classes.dex

Write-Host "=== 6. Zipaligning APK ==="
& "$BUILD_TOOLS\zipalign.exe" -f -p 4 build\apk_base.apk build\openroute_aligned.apk

Write-Host "=== 7. Signing APK (v1, v2, v3 schemes) ==="
& "$BUILD_TOOLS\apksigner.bat" sign `
    --ks build\debug.keystore `
    --ks-pass pass:android `
    --ks-key-alias androiddebugkey `
    --key-pass pass:android `
    --v1-signing-enabled true `
    --v2-signing-enabled true `
    --v3-signing-enabled true `
    --out build\apk\openroute.apk `
    build\openroute_aligned.apk

Write-Host "=== 8. Copying to deployment directories ==="
Copy-Item build\apk\openroute.apk apps\android\openroute.apk -Force
Copy-Item build\apk\openroute.apk build\bin\openroute.apk -Force -ErrorAction SilentlyContinue

Write-Host "=== 9. Verifying signed APK ==="
& "$BUILD_TOOLS\apksigner.bat" verify -v build\apk\openroute.apk

Write-Host "=== 10. Dumping badging ==="
& "$BUILD_TOOLS\aapt2.exe" dump badging apps\android\openroute.apk
