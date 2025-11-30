package lt.smworks.multiplatform3dengine

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.unit.dp
import lt.smworks.multiplatform3dengine.vulkan.EngineModelHandle

private const val TRANSLATE_STEP = 0.05f
private const val SCALE_STEP = 0.0005f
private const val ROTATE_STEP = 0.05f

@Composable
fun ModelControls(
    models: List<EngineModelHandle>,
    modifier: Modifier = Modifier
) {
    if (models.isEmpty()) {
        return
    }

    Column(
        modifier = modifier
            .fillMaxWidth()
            .background(Color(0x66000000), RoundedCornerShape(8.dp))
            .padding(12.dp)
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        TranslateControls(
            models = models,
            modifier = Modifier.fillMaxWidth()
        )
        ScaleControls(
            models = models,
            modifier = Modifier.fillMaxWidth()
        )
        RotateControls(
            models = models,
            modifier = Modifier.fillMaxWidth()
        )
    }
}

@Composable
private fun TranslateControls(
    models: List<EngineModelHandle>,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Text(
            text = "Translate",
            style = MaterialTheme.typography.titleSmall,
            color = Color.White
        )
        ControlRow(
            label = "X",
            modifier = Modifier.fillMaxWidth(),
            onDecrement = {
                models.forEach { handle ->
                    handle.translateBy(-TRANSLATE_STEP, 0f, 0f)
                }
            },
            onIncrement = {
                models.forEach { handle ->
                    handle.translateBy(TRANSLATE_STEP, 0f, 0f)
                }
            }
        )
        ControlRow(
            label = "Y",
            modifier = Modifier.fillMaxWidth(),
            onDecrement = {
                models.forEach { handle ->
                    handle.translateBy(0f, -TRANSLATE_STEP, 0f)
                }
            },
            onIncrement = {
                models.forEach { handle ->
                    handle.translateBy(0f, TRANSLATE_STEP, 0f)
                }
            }
        )
        ControlRow(
            label = "Z",
            modifier = Modifier.fillMaxWidth(),
            onDecrement = {
                models.forEach { handle ->
                    handle.translateBy(0f, 0f, -TRANSLATE_STEP)
                }
            },
            onIncrement = {
                models.forEach { handle ->
                    handle.translateBy(0f, 0f, TRANSLATE_STEP)
                }
            }
        )
    }
}

@Composable
private fun ScaleControls(
    models: List<EngineModelHandle>,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Text(
            text = "Scale",
            style = MaterialTheme.typography.titleSmall,
            color = Color.White
        )
        ControlRow(
            label = "Uniform",
            modifier = Modifier.fillMaxWidth(),
            onDecrement = {
                models.forEach { handle ->
                    handle.scaleBy(-SCALE_STEP)
                }
            },
            onIncrement = {
                models.forEach { handle ->
                    handle.scaleBy(SCALE_STEP)
                }
            }
        )
    }
}

@Composable
private fun RotateControls(
    models: List<EngineModelHandle>,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Text(
            text = "Rotate",
            style = MaterialTheme.typography.titleSmall,
            color = Color.White
        )
        ControlRow(
            label = "X",
            modifier = Modifier.fillMaxWidth(),
            onDecrement = {
                models.forEach { handle ->
                    handle.rotateBy(-ROTATE_STEP, 0f, 0f)
                }
            },
            onIncrement = {
                models.forEach { handle ->
                    handle.rotateBy(ROTATE_STEP, 0f, 0f)
                }
            }
        )
        ControlRow(
            label = "Y",
            modifier = Modifier.fillMaxWidth(),
            onDecrement = {
                models.forEach { handle ->
                    handle.rotateBy(0f, -ROTATE_STEP, 0f)
                }
            },
            onIncrement = {
                models.forEach { handle ->
                    handle.rotateBy(0f, ROTATE_STEP, 0f)
                }
            }
        )
        ControlRow(
            label = "Z",
            modifier = Modifier.fillMaxWidth(),
            onDecrement = {
                models.forEach { handle ->
                    handle.rotateBy(0f, 0f, -ROTATE_STEP)
                }
            },
            onIncrement = {
                models.forEach { handle ->
                    handle.rotateBy(0f, 0f, ROTATE_STEP)
                }
            }
        )
    }
}

@Composable
private fun ControlRow(
    label: String,
    modifier: Modifier = Modifier,
    onDecrement: () -> Unit,
    onIncrement: () -> Unit
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.bodyMedium,
            color = Color.White,
            modifier = Modifier.weight(1f)
        )
        Row(
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Button(onClick = onDecrement) {
                Text(text = "-")
            }
            Button(onClick = onIncrement) {
                Text(text = "+")
            }
        }
    }
}

