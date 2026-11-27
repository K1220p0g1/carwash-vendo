pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
        // Required for com.github.mik3y:usb-serial-for-android
        maven { url = uri("https://jitpack.io") }
    }
}

rootProject.name = "CarwashVendo"
include(":app")
