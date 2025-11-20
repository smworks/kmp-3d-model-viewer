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
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.unit.dp
import lt.smworks.multiplatform3dengine.vulkan.VulkanScreen
import lt.smworks.multiplatform3dengine.vulkan.VulkanSupport
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
        val context = LocalContext.current
        val supported = remember { VulkanSupport.isSupported(context) }

        if (supported) {
            val sceneState = rememberEngineScene {
                cameraDistance = 2.5f
                model("models/Box.gltf") {
                    scale(0.1f)
                    autoRotate(speedX = 0.01f, speedY = 0.06f)
                }
            }
            val fps by sceneState.fps

            Box(
                modifier = Modifier.fillMaxSize()
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
        } else {
            Text("Vulkan not supported on this device/emulator")
        }
    }
}

@Preview
@Composable
fun VulkanScreenAndroidPreview() {
    AndroidSample()
}