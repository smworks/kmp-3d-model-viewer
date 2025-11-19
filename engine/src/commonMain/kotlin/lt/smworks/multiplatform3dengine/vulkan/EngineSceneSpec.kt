package lt.smworks.multiplatform3dengine.vulkan

import androidx.compose.runtime.MutableState
import androidx.compose.runtime.State
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.Stable

@Stable
data class EngineSceneSpec(
    val cameraDistance: Float = 0f,
    val models: List<EngineModelSpec> = emptyList(),
    val fpsSamplePeriodMs: Long = 250L
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
class EngineSceneState internal constructor(
    val engine: EngineAPI,
    fpsState: MutableState<Int>
) {
    val fps: State<Int> = fpsState
}

class EngineSceneBuilder {
    var cameraDistance: Float = 0f
    var fpsSamplePeriodMs: Long = 250L
    private val models = mutableListOf<EngineModelSpec>()

    fun model(assetPath: String, block: EngineModelBuilder.() -> Unit = {}) {
        val builder = EngineModelBuilder(assetPath).apply(block)
        models += builder.build()
    }

    fun build(): EngineSceneSpec {
        return EngineSceneSpec(
            cameraDistance = cameraDistance,
            models = models.toList(),
            fpsSamplePeriodMs = fpsSamplePeriodMs
        )
    }
}

class EngineModelBuilder internal constructor(
    private val assetPath: String
) {
    private var translation = EngineModelTranslation()
    private var scale = 1f
    private var autoRotate: EngineModelAutoRotate? = null

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

    internal fun build(): EngineModelSpec {
        return EngineModelSpec(
            assetPath = assetPath,
            translation = translation,
            scale = scale,
            autoRotate = autoRotate
        )
    }
}

fun engineScene(block: EngineSceneBuilder.() -> Unit): EngineSceneSpec {
    val builder = EngineSceneBuilder()
    builder.block()
    return builder.build()
}

internal fun rememberFpsState(): MutableState<Int> = mutableStateOf(0)

