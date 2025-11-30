package lt.smworks.multiplatform3dengine.vulkan

import androidx.compose.runtime.Composable
import androidx.compose.runtime.Stable
import androidx.compose.ui.Modifier

@Stable
data class Config(
    val showFps: Boolean = false
)

@Composable
expect fun VulkanScreen(
	modifier: Modifier = Modifier,
	renderState: SceneRenderState,
	onUpdate: () -> Unit = {},
	config: Config = Config()
)
