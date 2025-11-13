package lt.smworks.multiplatform3dengine.vulkan

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier

@Composable
expect fun VulkanScreen(
	modifier: Modifier = Modifier
)

expect class VulkanRenderTarget


