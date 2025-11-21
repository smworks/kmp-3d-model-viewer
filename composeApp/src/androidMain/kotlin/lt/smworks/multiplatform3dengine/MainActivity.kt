package lt.smworks.multiplatform3dengine

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
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
                onUpdate {
                    scene.handle?.rotateBy(0.001f, 0.006f, 0.00f)
                }
            }
            val fps by sceneState.fps

            Box(
                modifier = Modifier.fillMaxSize()
            ) {
                VulkanScreen(
                    modifier = Modifier.fillMaxSize(),
                    engine = sceneState.engine,
                    onError = { error ->

                    }
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
}

@Preview
@Composable
fun VulkanScreenAndroidPreview() {
    AndroidSample()
}