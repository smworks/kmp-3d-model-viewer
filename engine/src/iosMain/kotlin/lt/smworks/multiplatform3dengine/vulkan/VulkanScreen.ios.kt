package lt.smworks.multiplatform3dengine.vulkan

import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier

@Composable
@Suppress("UNUSED_PARAMETER")
actual fun VulkanScreen(
	modifier: Modifier,
	renderState: SceneRenderState,
	onUpdate: () -> Unit = {}
) {
	Text("Vulkan screen is not available on iOS", modifier = modifier)
}


