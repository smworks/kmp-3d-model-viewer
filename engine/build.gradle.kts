plugins {
	alias(libs.plugins.kotlinMultiplatform)
	alias(libs.plugins.androidLibrary)
	alias(libs.plugins.composeMultiplatform)
	alias(libs.plugins.composeCompiler)
}

kotlin {
	androidTarget()

	listOf(
		iosArm64(),
		iosSimulatorArm64()
	)

	jvm()

	js {
		browser()
	}

	@OptIn(org.jetbrains.kotlin.gradle.ExperimentalWasmDsl::class)
	wasmJs {
		browser()
	}

	sourceSets {
		commonMain.dependencies {
			implementation(compose.runtime)
			implementation(compose.ui)
		}
		androidMain.dependencies {
			// AndroidView comes from compose.ui
		}
	}
}

android {
	namespace = "lt.smworks.multiplatform3dengine"
	compileSdk = libs.versions.android.compileSdk.get().toInt()

	defaultConfig {
		minSdk = libs.versions.android.minSdk.get().toInt()
		targetSdk = libs.versions.android.targetSdk.get().toInt()
	}
	externalNativeBuild {
		cmake {
			path = file("src/androidMain/cpp/CMakeLists.txt")
		}
	}
	ndkVersion = "26.3.11579264"
	packaging {
		resources {
			excludes += "/META-INF/{AL2.0,LGPL2.1}"
		}
	}
}


