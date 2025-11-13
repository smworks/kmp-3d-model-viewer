package lt.smworks.multiplatform3dengine

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import lt.smworks.multiplatform3dengine.vulkan.VulkanScreen
import lt.smworks.multiplatform3dengine.vulkan.VulkanSupport

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
			VulkanScreen(modifier = Modifier.fillMaxSize())
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