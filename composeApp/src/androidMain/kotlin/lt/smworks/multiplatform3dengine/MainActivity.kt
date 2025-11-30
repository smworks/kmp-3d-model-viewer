package lt.smworks.multiplatform3dengine

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.*
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import lt.smworks.multiplatform3dengine.vulkan.Config
import lt.smworks.multiplatform3dengine.vulkan.SceneRenderState
import lt.smworks.multiplatform3dengine.vulkan.VulkanScreen
import lt.smworks.multiplatform3dengine.vulkan.rememberSceneRenderer

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)

        setContent {
            AndroidSample()
        }
    }
}

@Composable
fun AndroidSample(
    viewModel: MainViewModel = viewModel()
) {
    MaterialTheme {
        val scene = viewModel.scene
        val renderState = rememberSceneRenderer(scene)

        Column(
            modifier = Modifier
                .fillMaxSize()
                .systemBarsPadding()
        ) {
            ModelPreview(
                renderState = renderState,
                onUpdate = viewModel::onUpdate
            )
            ModelControls(
                hasModels = scene.models.isNotEmpty(),
                onTranslate = viewModel::translateModels,
                onScale = viewModel::scaleModels,
                onRotate = viewModel::rotateModels,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 12.dp)
            )
        }
    }
}

@Composable
private fun ColumnScope.ModelPreview(
    renderState: SceneRenderState,
    onUpdate: () -> Unit
) {
    Box(
        modifier = Modifier
            .weight(1f)
            .fillMaxSize()
    ) {
        VulkanScreen(
            modifier = Modifier.fillMaxSize(),
            renderState = renderState,
            onUpdate = onUpdate,
            config = Config(showFps = true)
        )
    }
}