This is a Kotlin Multiplatform project targeting Android and iOS.

* [`/engine`](./engine/src) is the multiplatform rendering engine. This module is the one you package and ship as a library.

* [`/composeApp`](./composeApp/src) is a demo client that exercises the engine with a Compose Multiplatform UI. It contains several subfolders:
    - [`commonMain`](./composeApp/src/commonMain/kotlin) holds the shared Kotlin code used by the demo.
    - [`androidMain`](./composeApp/src/androidMain/kotlin) contains Android-specific demo implementations.
    - [`iosMain`](./composeApp/src/iosMain/kotlin) contains iOS-specific demo implementations (for example, CoreCrypto calls).

* [`/iosApp`](./iosApp/iosApp) is a demo iOS application that integrates the engine. Open it in Xcode to run the demo or add SwiftUI experiments.

### Build and Run Android Application

To build and run the development version of the Android app, use the run configuration from the run widget
in your IDE’s toolbar or build it directly from the terminal:

- on macOS/Linux
  ```shell
  ./gradlew :composeApp:assembleDebug
  ```
- on Windows
  ```shell
  .\gradlew.bat :composeApp:assembleDebug
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