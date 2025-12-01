package lt.smworks.multiplatform3dengine.vulkan

import androidx.compose.runtime.Composable
import androidx.compose.runtime.Stable
import androidx.compose.ui.Modifier

@Stable
data class Config(
    val showFps: Boolean = false,
    val targetFps: TargetFPS = TargetFPS.FPS_VSYNC
)

enum class TargetFPS {
    FPS_24,
    FPS_30,
    FPS_60,
    FPS_UNLIMITED,
    FPS_VSYNC
}

@Composable
expect fun VulkanScreen(
	modifier: Modifier = Modifier,
	scene: Scene,
	onUpdate: () -> Unit = {},
	config: Config = Config()
)
