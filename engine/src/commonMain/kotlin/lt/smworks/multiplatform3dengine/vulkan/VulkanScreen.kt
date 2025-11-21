package lt.smworks.multiplatform3dengine.vulkan

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier

@Composable
expect fun VulkanScreen(
	modifier: Modifier = Modifier,
	engine: EngineAPI,
    onError: (error: String) -> Unit
)

expect class VulkanRenderTarget


