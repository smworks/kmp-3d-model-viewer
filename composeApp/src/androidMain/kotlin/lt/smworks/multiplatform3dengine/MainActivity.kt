package lt.smworks.multiplatform3dengine

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.unit.dp
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
        val scene by viewModel.scene.collectAsStateWithLifecycle()
        val renderState = rememberSceneRenderer(scene)
        val fps by renderState.fps

        Column(
            modifier = Modifier
                .fillMaxSize()
                .systemBarsPadding()
        ) {
            ModelPreview(
                renderState = renderState,
                fps = fps,
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
    fps: Int,
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
            onUpdate = onUpdate
        )

        FPSCounter(fps)
    }
}

@Composable
private fun BoxScope.FPSCounter(fps: Int) {
    Text(
        text = "$fps FPS",
        modifier = Modifier
            .align(Alignment.TopEnd)
            .padding(16.dp)
            .background(
                color = Color(0x66000000),
                shape = RoundedCornerShape(8.dp)
            )
            .padding(horizontal = 12.dp, vertical = 6.dp),
        color = Color.White,
        style = MaterialTheme.typography.bodyMedium
    )
}