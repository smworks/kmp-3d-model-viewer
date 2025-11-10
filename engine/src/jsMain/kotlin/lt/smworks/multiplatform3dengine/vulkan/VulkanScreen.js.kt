package lt.smworks.multiplatform3dengine.vulkan

import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier

actual class VulkanRenderTarget

@Composable
actual fun VulkanScreen(
	modifier: Modifier,
	onCreate: (VulkanRenderTarget) -> Unit,
	onDestroy: () -> Unit
) {
	Text("Vulkan screen is not supported on JS")
}


