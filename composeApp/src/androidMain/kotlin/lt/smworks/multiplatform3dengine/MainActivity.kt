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
import androidx.compose.material3.Text
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.platform.LocalContext
import lt.smworks.multiplatform3dengine.vulkan.VulkanSupport
import android.content.res.AssetManager

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
		val renderer = remember { VulkanNativeRenderer() }
		DisposableEffect(Unit) {
			onDispose {
				renderer.destroy()
			}
		}
		if (supported) {
			// Setup renderer for gesture handling
			renderer.setupForGestures()
			
			VulkanScreen(
				modifier = Modifier.fillMaxSize(),
				onCreate = { surface ->
					renderer.init(surface, context.assets)
					renderer.start()
				},
				onDestroy = {
					renderer.destroy()
				}
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