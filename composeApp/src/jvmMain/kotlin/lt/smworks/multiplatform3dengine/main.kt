package lt.smworks.multiplatform3dengine

import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application

fun main() = application {
    Window(
        onCloseRequest = ::exitApplication,
        title = "multiplatform3dengine",
    ) {
        App()
    }
}