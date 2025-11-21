package lt.smworks.multiplatform3dengine.vulkan

import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier

actual class VulkanRenderTarget

@Composable
@Suppress("UNUSED_PARAMETER")
actual fun VulkanScreen(
	modifier: Modifier,
	engine: EngineAPI,
    onError: (error: String) -> Unit
) {
	Text("Vulkan screen is not available on iOS", modifier = modifier)
}


