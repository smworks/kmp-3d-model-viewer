This repository contains a multiplatform rendering engine and a native Android demo application.

* [`/engine`](./engine/src) is the multiplatform rendering engine. This module is the one you package and ship as a library.

* [`/app`](./app/src) is a native Android demo client that exercises the engine with a Jetpack Compose UI. Its sources live under the standard `src/main` Android source set.

* [`/iosApp`](./iosApp/iosApp) is a demo iOS application that integrates the engine. Open it in Xcode to run the demo or add SwiftUI experiments.

### Build and Run Android Application

To build and run the development version of the Android app, use the run configuration from the run widget
in your IDE’s toolbar or build it directly from the terminal:

- on macOS/Linux
  ```shell
  ./gradlew :app:assembleDebug
  ```
- on Windows
  ```shell
  .\gradlew.bat :app:assembleDebug
  ```

### Build and Run iOS Application

To build and run the development version of the iOS app, use the run configuration from the run widget
in your IDE’s toolbar or open the [/iosApp](./iosApp) directory in Xcode and run it from there.

## Development Guidelines

- Prefer simple, easy-to-read implementations over clever or overly optimized ones.
- Avoid comments unless they are extremely necessary; rely on self-explanatory code.
- Use Hungarian notation for all C++ code in this repository.
- Follow Kotlin's official coding conventions for Kotlin sources.

Refer to [`AGENTS.md`](./AGENTS.md) for the canonical set of agent instructions.

---

Learn more about [Kotlin Multiplatform](https://www.jetbrains.com/help/kotlin-multiplatform-dev/get-started.html)
and [Compose Multiplatform](https://github.com/JetBrains/compose-multiplatform/#compose-multiplatform).