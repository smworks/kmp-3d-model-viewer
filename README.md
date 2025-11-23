This is a Kotlin Multiplatform project targeting Android and iOS.

* [/composeApp](./composeApp/src) is for code that will be shared across your Android and iOS applications.
  It contains several subfolders:
    - [commonMain](./composeApp/src/commonMain/kotlin) is for shared Kotlin code.
    - [androidMain](./composeApp/src/androidMain/kotlin) contains Android-specific implementations.
    - [iosMain](./composeApp/src/iosMain/kotlin) contains iOS-specific implementations (for example, CoreCrypto calls).

* [/iosApp](./iosApp/iosApp) contains iOS applications. Even if you’re sharing your UI with Compose Multiplatform,
  you need this entry point for your iOS app. This is also where you should add SwiftUI code for your project.

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