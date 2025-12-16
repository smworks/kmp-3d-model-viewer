package lt.smworks.multiplatform3dengine

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import lt.smworks.multiplatform3dengine.vulkan.Scene
import lt.smworks.multiplatform3dengine.vulkan.SceneCamera
import lt.smworks.multiplatform3dengine.vulkan.SceneModel
import lt.smworks.multiplatform3dengine.vulkan.SceneVector3

private const val MODEL_ID = "main_scene"

class MainViewModel : ViewModel() {
    var scene by mutableStateOf(initialScene())
        private set

    private fun initialScene(): Scene = Scene(
        camera = SceneCamera(
            position = SceneVector3(z = 1.5f),
            rotation = SceneVector3(x = -0.2f)
        ),
        models = listOf(
            SceneModel(
                id = MODEL_ID,
                assetPath = "models/scene.gltf",
                scale = 0.001f
            )
        ).associateBy(SceneModel::id)
    )

    fun onUpdate() {
        scene = scene.rotateModelBy(MODEL_ID, 0.001f, 0.001f, 0.001f)
    }

    fun translateModel(deltaX: Float, deltaY: Float, deltaZ: Float) {
        scene = scene.translateModelBy(MODEL_ID, deltaX, deltaY, deltaZ)
    }

    fun scaleModel(delta: Float) {
        scene = scene.scaleModelBy(MODEL_ID, delta)
    }

    fun rotateModel(deltaX: Float, deltaY: Float, deltaZ: Float) {
        scene = scene.rotateModelBy(MODEL_ID, deltaX, deltaY, deltaZ)
    }
}
