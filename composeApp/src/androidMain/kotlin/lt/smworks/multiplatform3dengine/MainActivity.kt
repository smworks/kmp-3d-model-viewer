package lt.smworks.multiplatform3dengine

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
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
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.unit.dp
import lt.smworks.multiplatform3dengine.vulkan.EngineSceneState
import lt.smworks.multiplatform3dengine.vulkan.VulkanScreen
import lt.smworks.multiplatform3dengine.vulkan.rememberEngineScene

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
fun AndroidSample() {
    MaterialTheme {
        val sceneState = rememberEngineScene {
            cameraPosition(z = 1.5f)
            cameraRotation(x = -0.2f)
            val scene = model("models/scene.gltf") {
                scale(0.001f)
            }
            scene.onUpdate {
//                rotateBy(0.001f, 0.006f, 0.0f)
            }
        }
        val modelHandles = sceneState.models.values.toList()
        val fps by sceneState.fps

        Column(
            modifier = Modifier
                .fillMaxSize()
                .systemBarsPadding()
        ) {
            ModelPreview(sceneState, fps)
            ModelControls(
                models = modelHandles,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 12.dp)
            )
        }
    }
}

@Composable
private fun ColumnScope.ModelPreview(
    sceneState: EngineSceneState,
    fps: Int
) {
    Box(
        modifier = Modifier
            .weight(1f)
            .fillMaxSize()
    ) {
        VulkanScreen(
            modifier = Modifier.fillMaxSize(),
            engine = sceneState.engine
        )

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
}

@Preview
@Composable
fun VulkanScreenAndroidPreview() {
    AndroidSample()
}