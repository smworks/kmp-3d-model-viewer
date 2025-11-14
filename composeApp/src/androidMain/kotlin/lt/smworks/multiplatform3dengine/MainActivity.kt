package lt.smworks.multiplatform3dengine

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import lt.smworks.multiplatform3dengine.vulkan.VulkanScreen
import lt.smworks.multiplatform3dengine.vulkan.VulkanSupport
import lt.smworks.multiplatform3dengine.vulkan.rememberEngineApi

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
            val engine = rememberEngineApi()
            LaunchedEffect(Unit) {
                engine.moveCamera(-1.1f)
                val modelPath = "models/cube-tex.obj"
                val modelId = engine.loadModel(modelPath, 0f, 0f, 0f, 1f)
                engine.rotate(modelId, 0.0f, 180.0f, 0.0f)
            }

            VulkanScreen(
                modifier = Modifier.fillMaxSize(),
                engine = engine
            )
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