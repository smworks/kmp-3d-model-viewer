package lt.smworks.multiplatform3dengine

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import lt.smworks.multiplatform3dengine.vulkan.Config
import lt.smworks.multiplatform3dengine.vulkan.VulkanScreen

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

        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black)
                .systemBarsPadding()
        ) {
            VulkanScreen(
                modifier = Modifier.weight(1f).fillMaxWidth(),
                scene = scene,
                onUpdate = viewModel::onUpdate,
                config = Config(showFps = true)
            )
            ModelControls(
                hasModels = scene.models.isNotEmpty(),
                onTranslate = viewModel::translateModel,
                onScale = viewModel::scaleModel,
                onRotate = viewModel::rotateModel,
                modifier = Modifier
                    .weight(1f)
                    .padding(horizontal = 16.dp, vertical = 12.dp)
            )
        }
    }
}