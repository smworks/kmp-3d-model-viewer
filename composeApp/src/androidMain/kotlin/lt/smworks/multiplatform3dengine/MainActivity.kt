package lt.smworks.multiplatform3dengine

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.foundation.layout.fillMaxSize
import lt.smworks.multiplatform3dengine.vulkan.VulkanScreen
import lt.smworks.multiplatform3dengine.vulkan.VulkanNativeRenderer
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.remember

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
		val renderer = remember { VulkanNativeRenderer() }
		DisposableEffect(Unit) {
			onDispose {
				renderer.destroy()
			}
		}
		VulkanScreen(
			modifier = Modifier.fillMaxSize(),
			onCreate = { surface ->
				renderer.init(surface)
				renderer.start()
			},
			onDestroy = {
				renderer.destroy()
			}
		)
	}
}

@Preview
@Composable
fun VulkanScreenAndroidPreview() {
	AndroidSample()
}