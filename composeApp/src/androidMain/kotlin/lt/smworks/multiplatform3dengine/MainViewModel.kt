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
        val model = scene.getModel(MODEL_ID) ?: return
        scene.updateModel(model)
    }

    fun translateModels(deltaX: Float, deltaY: Float, deltaZ: Float) {
        val currentScene = scene
        scene = currentScene.copy(
            models = currentScene.models.mapValues { (_, model) ->
                model.copy(
                    translation = model.translation.offset(deltaX, deltaY, deltaZ)
                )
            }
        )
    }

    fun scaleModels(delta: Float) {
        val currentScene = scene
        scene = currentScene.copy(
            models = currentScene.models.mapValues { (_, model) ->
                val newScale = (model.scale + delta).coerceAtLeast(0.0001f)
                model.copy(scale = newScale)
            }
        )
    }

    fun rotateModels(deltaX: Float, deltaY: Float, deltaZ: Float) {
        val currentScene = scene
        scene = currentScene.copy(
            models = currentScene.models.mapValues { (_, model) ->
                model.copy(
                    rotation = model.rotation.offset(deltaX, deltaY, deltaZ)
                )
            }
        )
    }
}

private fun SceneVector3.offset(deltaX: Float, deltaY: Float, deltaZ: Float): SceneVector3 {
    return SceneVector3(
        x = x + deltaX,
        y = y + deltaY,
        z = z + deltaZ
    )
}


