package lt.smworks.multiplatform3dengine.vulkan

import androidx.compose.runtime.MutableState
import androidx.compose.runtime.State
import androidx.compose.runtime.Stable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.snapshots.SnapshotStateMap
import kotlinx.coroutines.flow.MutableSharedFlow

@Stable
data class Scene(
    val camera: SceneCamera = SceneCamera(),
    val models: Map<String, SceneModel> = emptyMap()
) {
    fun getModel(id: String): SceneModel? = models[id]

    fun updateModel(model: SceneModel): Scene {
        val targetId = model.id
        return copy(models = models + (targetId to model))
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
)

@Stable
data class SceneModel(
    val id: String,
    val assetPath: String,
    val translation: SceneVector3 = SceneVector3(),
    val rotation: SceneVector3 = SceneVector3(),
    val scale: Float = 1f,
    val autoRotate: SceneModelAutoRotate? = null,
    val onUpdate: (EngineModelHandle.() -> Unit)? = null
)

@Stable
data class SceneModelAutoRotate(
    val speed: SceneVector3 = SceneVector3(),
    val intervalMs: Long = 16L
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

internal fun rememberFpsState(): MutableState<Int> = mutableStateOf(0)

@Stable
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

@Stable
class SceneUpdateScope internal constructor(
    private val handles: SnapshotStateMap<String, EngineModelHandle>
) {
    val models: Collection<EngineModelHandle>
        get() = handles.values

    fun modelById(id: String): EngineModelHandle? = handles[id]

    fun modelByAsset(assetPath: String): EngineModelHandle? {
        return handles.values.firstOrNull { handle -> handle.assetPath == assetPath }
    }

    fun forEach(action: (EngineModelHandle) -> Unit) {
        handles.values.forEach(action)
    }
}


