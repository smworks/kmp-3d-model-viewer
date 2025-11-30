package lt.smworks.multiplatform3dengine.vulkan

import androidx.compose.runtime.MutableState
import androidx.compose.runtime.State
import androidx.compose.runtime.Stable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.snapshots.SnapshotStateMap

@Stable
data class EngineSceneSpec(
    val cameraPosition: EngineCameraPosition = EngineCameraPosition(),
    val cameraRotation: EngineCameraRotation = EngineCameraRotation(),
    val models: List<EngineModelHandleRef> = emptyList(),
    val fpsSamplePeriodMs: Long = 250L,
    val modelUpdates: Map<EngineModelHandleRef, EngineModelHandle.() -> Unit> = emptyMap(),
    val onUpdate: (EngineSceneUpdateScope.() -> Unit)? = null
)

@Stable
data class EngineCameraPosition(
    val x: Float = 0f,
    val y: Float = 0f,
    val z: Float = 4f
)

@Stable
data class EngineCameraRotation(
    val x: Float = 0f,
    val y: Float = 0f,
    val z: Float = 0f
)

@Stable
data class EngineModelSpec(
    val assetPath: String,
    val translation: EngineModelTranslation = EngineModelTranslation(),
    val scale: Float = 1f,
    val autoRotate: EngineModelAutoRotate? = null
)

@Stable
data class EngineModelTranslation(
    val x: Float = 0f,
    val y: Float = 0f,
    val z: Float = 0f
)

@Stable
data class EngineModelAutoRotate(
    val speedX: Float = 0f,
    val speedY: Float = 0f,
    val speedZ: Float = 0f,
    val intervalMs: Long = 16L
)

@Stable
class EngineModelHandleRef internal constructor(
    internal val spec: EngineModelSpec
) {
    internal fun isSameSpec(other: EngineModelSpec): Boolean = spec == other

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is EngineModelHandleRef) return false
        return spec == other.spec
    }

    override fun hashCode(): Int = spec.hashCode()
}

@Stable
class EngineSceneState internal constructor(
    val engine: EngineAPI,
    fpsState: MutableState<Int>,
    internal val modelHandles: SnapshotStateMap<EngineModelHandleRef, EngineModelHandle>
) {
    val fps: State<Int> = fpsState

    val models: Map<EngineModelHandleRef, EngineModelHandle>
        get() = modelHandles

    val EngineModelHandleRef.handle: EngineModelHandle?
        get() = modelHandles[this]

    fun model(assetPath: String): EngineModelHandle? {
        return modelHandles.values.firstOrNull { handle -> handle.assetPath == assetPath }
    }

    fun model(handleRef: EngineModelHandleRef): EngineModelHandle? {
        return modelHandles[handleRef]
    }

    fun model(spec: EngineModelSpec): EngineModelHandle? {
        return modelHandles.entries.firstOrNull { entry -> entry.key.isSameSpec(spec) }?.value
    }
}

class EngineSceneBuilder {
    private var cameraPosition = EngineCameraPosition()
    private var cameraRotation = EngineCameraRotation()
    var fpsSamplePeriodMs: Long = 250L
    private val modelRefs = mutableListOf<EngineModelHandleRef>()
    private var onUpdate: (EngineSceneUpdateScope.() -> Unit)? = null
    private val modelUpdates = mutableMapOf<EngineModelHandleRef, EngineModelHandle.() -> Unit>()

    fun cameraPosition(x: Float = 0f, y: Float = 0f, z: Float = 4f) {
        cameraPosition = EngineCameraPosition(x, y, z)
    }

    fun cameraRotation(x: Float = 0f, y: Float = 0f, z: Float = 0f) {
        cameraRotation = EngineCameraRotation(x, y, z)
    }

    fun model(assetPath: String, block: EngineModelBuilder.() -> Unit = {}): EngineModelHandleRef {
        val builder = EngineModelBuilder(assetPath).apply(block)
        val spec = builder.build()
        val handleRef = EngineModelHandleRef(spec).also(modelRefs::add)
        builder.consumeOnUpdate()?.let { updateBlock ->
            modelUpdates[handleRef] = updateBlock
        }
        return handleRef
    }

    fun onUpdate(block: EngineSceneUpdateScope.() -> Unit) {
        onUpdate = block
    }

    fun EngineModelHandleRef.onUpdate(block: EngineModelHandle.() -> Unit) {
        modelUpdates[this] = block
    }

    fun build(): EngineSceneSpec {
        return EngineSceneSpec(
            cameraPosition = cameraPosition,
            cameraRotation = cameraRotation,
            models = modelRefs.toList(),
            fpsSamplePeriodMs = fpsSamplePeriodMs,
            modelUpdates = modelUpdates.toMap(),
            onUpdate = onUpdate
        )
    }
}

class EngineModelBuilder internal constructor(
    private val assetPath: String
) {
    private var translation = EngineModelTranslation()
    private var scale = 1f
    private var autoRotate: EngineModelAutoRotate? = null
    private var updateBlock: (EngineModelHandle.() -> Unit)? = null

    fun position(x: Float = 0f, y: Float = 0f, z: Float = 0f) {
        translation = EngineModelTranslation(x, y, z)
    }

    fun scale(value: Float) {
        scale = value
    }

    fun autoRotate(
        speedX: Float = 0f,
        speedY: Float = 0f,
        speedZ: Float = 0f,
        intervalMs: Long = 16L
    ) {
        autoRotate = EngineModelAutoRotate(speedX, speedY, speedZ, intervalMs)
    }

    fun onUpdate(block: EngineModelHandle.() -> Unit) {
        updateBlock = block
    }

    internal fun build(): EngineModelSpec {
        return EngineModelSpec(
            assetPath = assetPath,
            translation = translation,
            scale = scale,
            autoRotate = autoRotate
        )
    }

    internal fun consumeOnUpdate(): (EngineModelHandle.() -> Unit)? = updateBlock
}

fun engineScene(block: EngineSceneBuilder.() -> Unit): EngineSceneSpec {
    val builder = EngineSceneBuilder()
    builder.block()
    return builder.build()
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
class EngineSceneUpdateScope internal constructor(
    private val handles: SnapshotStateMap<EngineModelHandleRef, EngineModelHandle>
) {
    val models: Collection<EngineModelHandle>
        get() = handles.values

    val EngineModelHandleRef.handle: EngineModelHandle?
        get() = handles[this]

    fun model(handleRef: EngineModelHandleRef): EngineModelHandle? = handles[handleRef]

    fun model(spec: EngineModelSpec): EngineModelHandle? {
        return handles.entries.firstOrNull { entry -> entry.key.isSameSpec(spec) }?.value
    }

    fun model(assetPath: String): EngineModelHandle? {
        return handles.values.firstOrNull { handle -> handle.assetPath == assetPath }
    }

    fun forEach(action: (EngineModelHandle) -> Unit) {
        handles.values.forEach(action)
    }
}

