package lt.smworks.multiplatform3dengine

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import lt.smworks.multiplatform3dengine.vulkan.Scene
import lt.smworks.multiplatform3dengine.vulkan.SceneCamera
import lt.smworks.multiplatform3dengine.vulkan.SceneModel
import lt.smworks.multiplatform3dengine.vulkan.SceneVector3

class MainViewModel : ViewModel() {
    private val _scene = MutableStateFlow(initialScene())
    val scene: StateFlow<Scene> = _scene.asStateFlow()

    fun translateModels(deltaX: Float, deltaY: Float, deltaZ: Float) {
        _scene.update { current ->
            current.copy(
                models = current.models.map { model ->
                    model.copy(
                        translation = model.translation.offset(deltaX, deltaY, deltaZ)
                    )
                }
            )
        }
    }

    fun scaleModels(delta: Float) {
        _scene.update { current ->
            current.copy(
                models = current.models.map { model ->
                    val newScale = (model.scale + delta).coerceAtLeast(0.0001f)
                    model.copy(scale = newScale)
                }
            )
        }
    }

    fun rotateModels(deltaX: Float, deltaY: Float, deltaZ: Float) {
        _scene.update { current ->
            current.copy(
                models = current.models.map { model ->
                    model.copy(
                        rotation = model.rotation.offset(deltaX, deltaY, deltaZ)
                    )
                }
            )
        }
    }

    fun onUpdate() {
    }

    private fun initialScene(): Scene = Scene(
        camera = SceneCamera(
            position = SceneVector3(z = 1.5f),
            rotation = SceneVector3(x = -0.2f)
        ),
        models = listOf(
            SceneModel(
                id = "main_scene",
                assetPath = "models/scene.gltf",
                scale = 0.001f
            )
        )
    )
}

private fun SceneVector3.offset(deltaX: Float, deltaY: Float, deltaZ: Float): SceneVector3 {
    return SceneVector3(
        x = x + deltaX,
        y = y + deltaY,
        z = z + deltaZ
    )
}


