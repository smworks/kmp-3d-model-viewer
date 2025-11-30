package lt.smworks.multiplatform3dengine.vulkan

import androidx.compose.runtime.MutableState
import androidx.compose.runtime.State
import androidx.compose.runtime.Stable
import androidx.compose.runtime.snapshots.SnapshotStateMap
import kotlinx.coroutines.flow.MutableSharedFlow

@Stable
data class Scene(
    val camera: SceneCamera = SceneCamera(),
    val models: Map<String, SceneModel> = emptyMap()
) {
    fun getModel(id: String): SceneModel? = models[id]

    fun updateModel(id: String, updater: (SceneModel) -> SceneModel): Scene {
        val existingModel = models[id] ?: return this
        val updatedModel = updater(existingModel)
        if (updatedModel == existingModel) {
            return this
        }
        return copy(models = models + (id to updatedModel))
    }

    fun translateModelBy(id: String, deltaX: Float, deltaY: Float, deltaZ: Float): Scene {
        return updateModel(id) { model ->
            model.copy(translation = model.translation.offsetBy(deltaX, deltaY, deltaZ))
        }
    }

    fun setModelTranslation(id: String, translation: SceneVector3): Scene {
        return updateModel(id) { model ->
            model.copy(translation = translation)
        }
    }

    fun setModelTranslation(id: String, x: Float, y: Float, z: Float): Scene {
        return setModelTranslation(id, SceneVector3(x = x, y = y, z = z))
    }

    fun rotateModelBy(id: String, deltaX: Float, deltaY: Float, deltaZ: Float): Scene {
        return updateModel(id) { model ->
            model.copy(rotation = model.rotation.offsetBy(deltaX, deltaY, deltaZ))
        }
    }

    fun setModelRotation(id: String, rotation: SceneVector3): Scene {
        return updateModel(id) { model ->
            model.copy(rotation = rotation)
        }
    }

    fun setModelRotation(id: String, x: Float, y: Float, z: Float): Scene {
        return setModelRotation(id, SceneVector3(x = x, y = y, z = z))
    }

    fun scaleModelBy(id: String, delta: Float): Scene {
        return updateModel(id) { model ->
            model.copy(scale = (model.scale + delta).coerceAtLeast(MIN_SCALE))
        }
    }

    fun setModelScale(id: String, scale: Float): Scene {
        return updateModel(id) { model ->
            model.copy(scale = scale.coerceAtLeast(MIN_SCALE))
        }
    }

    fun updateModel(model: SceneModel): Scene {
        val targetId = model.id
        return copy(models = models + (targetId to model))
    }

    companion object {
        private const val MIN_SCALE = 0.0001f
    }
}

@Stable
data class SceneCamera(
    val position: SceneVector3 = SceneVector3(z = 4f),
    val rotation: SceneVector3 = SceneVector3()
)

@Stable
data class SceneVector3(
    val x: Float = 0f,
    val y: Float = 0f,
    val z: Float = 0f
) {
    fun offsetBy(deltaX: Float, deltaY: Float, deltaZ: Float): SceneVector3 {
        return copy(
            x = x + deltaX,
            y = y + deltaY,
            z = z + deltaZ
        )
    }
}

@Stable
data class SceneModel(
    val id: String,
    val assetPath: String,
    val translation: SceneVector3 = SceneVector3(),
    val rotation: SceneVector3 = SceneVector3(),
    val scale: Float = 1f,
    val onUpdate: (EngineModelHandle.() -> Unit)? = null
)

@Stable
class SceneRenderState internal constructor(
    val engine: EngineAPI,
    fpsState: MutableState<Int>,
    internal val modelHandles: SnapshotStateMap<String, EngineModelHandle>,
    internal val frameUpdates: MutableSharedFlow<Unit>
) {
    val fps: State<Int> = fpsState

    val models: Map<String, EngineModelHandle>
        get() = modelHandles

    fun modelById(id: String): EngineModelHandle? = modelHandles[id]

    fun modelByAsset(assetPath: String): EngineModelHandle? {
        return modelHandles.values.firstOrNull { handle -> handle.assetPath == assetPath }
    }

    internal fun notifyFrameRendered() {
        frameUpdates.tryEmit(Unit)
    }
}

class EngineModelHandle internal constructor(
    val id: Long,
    val assetPath: String,
    private val engine: EngineAPI
) {
    fun translateTo(x: Float, y: Float, z: Float) {
        engine.translate(id, x, y, z)
    }

    fun translateBy(deltaX: Float, deltaY: Float, deltaZ: Float) {
        engine.translateBy(id, deltaX, deltaY, deltaZ)
    }

    fun scaleTo(value: Float) {
        engine.scale(id, value)
    }

    fun scaleBy(delta: Float) {
        engine.scaleBy(id, delta)
    }

    fun rotateTo(rotationX: Float, rotationY: Float, rotationZ: Float) {
        engine.rotate(id, rotationX, rotationY, rotationZ)
    }

    fun rotateBy(deltaX: Float, deltaY: Float, deltaZ: Float) {
        engine.rotateBy(id, deltaX, deltaY, deltaZ)
    }
}


