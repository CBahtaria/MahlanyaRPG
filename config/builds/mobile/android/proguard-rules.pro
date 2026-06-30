# MahlanyaRPG ProGuard Rules for UE5 Android Build
# Applied during AAB packaging to strip unused code.

# Keep UE5 JNI bridge classes
-keep class com.epicgames.** { *; }
-keep class com.mahlanyarpg.** { *; }

# Keep Google Play Asset Delivery classes
-keep class com.google.android.play.core.** { *; }

# Keep crash reporter
-keep class com.epicgames.unreal.CrashReportClient { *; }

# Keep SaveGame reflection targets
-keepclassmembers class ** implements android.os.Parcelable { *; }

# Strip debug logging in Release
-assumenosideeffects class android.util.Log {
    public static int v(...);
    public static int d(...);
}

# Obfuscate everything else
-optimizationpasses 5
-dontusemixedcaseclassnames
-dontskipnonpubliclibraryclasses
-verbose
