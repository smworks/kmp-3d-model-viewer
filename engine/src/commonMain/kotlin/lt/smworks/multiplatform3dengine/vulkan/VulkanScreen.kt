package lt.smworks.multiplatform3dengine.vulkan

import androidx.compose.runtime.Composable
import androidx.compose.runtime.Stable
import androidx.compose.ui.Modifier

@Stable
data class Config(
    val showFps: Boolean = false,
    val fpsSamplePeriodMs: Long = 250L
)

@Composable
expect fun VulkanScreen(
	modifier: Modifier = Modifier,
	scene: Scene,
	onUpdate: () -> Unit = {},
	config: Config = Config()
)
