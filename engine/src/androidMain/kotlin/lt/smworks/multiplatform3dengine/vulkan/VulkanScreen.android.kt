package lt.smworks.multiplatform3dengine.vulkan

import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.launch
import kotlin.math.PI

private const val LOG_TAG = "VulkanScreen"
private const val MODEL_UPDATE_INTERVAL_MS = 16L

private var currentRenderer: EngineAPI? = null

internal fun setCurrentRendererForGestures(renderer: EngineAPI) {
    currentRenderer = renderer
}

@Composable
actual fun VulkanScreen(
    modifier: Modifier,
    scene: Scene,
    onUpdate: () -> Unit,
    config: Config
) {
    val renderState = rememberSceneRenderer(scene, fpsSampler = config.fpsSamplePeriodMs)
    val engine = renderState.engine
    val context = LocalContext.current
    val supported = remember { VulkanSupport.isSupported(context) }
    DisposableEffect(engine, renderState) {
        engine.setOnFrameUpdate {
            renderState.notifyFrameRendered()
        }
        onDispose {
            engine.setOnFrameUpdate(null)
        }
    }

    LaunchedEffect(renderState, onUpdate) {
        renderState.frameUpdates.collect {
            onUpdate()
        }
    }

    if (supported) {
        Box(modifier = modifier) {
            AndroidView(
                modifier = Modifier.matchParentSize(),
                factory = { viewContext ->
                    SurfaceView(viewContext).apply {
                        var activePointerId = MotionEvent.INVALID_POINTER_ID
                        var lastSurfaceWidth = -1
                        var lastSurfaceHeight = -1
                        var lastLayoutWidth = -1
                        var lastLayoutHeight = -1
                        var lastOrientation: String? = null

                        addOnLayoutChangeListener { _, left, top, right, bottom, _, _, _, _ ->
                            val layoutWidth = right - left
                            val layoutHeight = bottom - top
                            if (layoutWidth <= 0 || layoutHeight <= 0) {
                                return@addOnLayoutChangeListener
                            }
                            if (layoutWidth != lastLayoutWidth || layoutHeight != lastLayoutHeight) {
                                lastLayoutWidth = layoutWidth
                                lastLayoutHeight = layoutHeight
                                Log.i(LOG_TAG, "Layout changed to ${layoutWidth}x$layoutHeight")
                                engine.resize(layoutWidth, layoutHeight)
                            }
                        }

                        holder.addCallback(object : SurfaceHolder.Callback {
                            override fun surfaceCreated(holder: SurfaceHolder) {
                                val surface = holder.surface
                                if (surface != null && surface.isValid) {
                                    engine.init(surface, viewContext.assets)
                                    engine.start()
                                }
                            }

                            override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                                val orientation = when {
                                    width > height -> "landscape"
                                    height > width -> "portrait"
                                    else -> "square"
                                }

                                if (orientation != lastOrientation) {
                                    Log.i(LOG_TAG, "Orientation changed to $orientation (width=$width, height=$height)")
                                    lastOrientation = orientation
                                }

                                if (width != lastSurfaceWidth || height != lastSurfaceHeight) {
                                    engine.resize(width, height)
                                    lastSurfaceWidth = width
                                    lastSurfaceHeight = height
                                    Log.i(LOG_TAG, "Surface resized to ${width}x$height")
                                }
                            }

                            override fun surfaceDestroyed(holder: SurfaceHolder) {
                                lastSurfaceWidth = -1
                                lastSurfaceHeight = -1
                                lastLayoutWidth = -1
                                lastLayoutHeight = -1
                                lastOrientation = null
                                engine.destroy()
                            }
                        })
                    }
                }
            )

            if (config.showFps) {
                val fps by renderState.fps
                FpsCounter(fps)
            }
        }
    } else {
        Text(
            text = "Vulkan is not supported",
            modifier = modifier
        )
    }
}

@Composable
private fun BoxScope.FpsCounter(fps: Int) {
    Text(
        text = "$fps FPS",
        modifier = Modifier
            .align(Alignment.TopEnd)
            .padding(16.dp)
            .background(
                color = Color(0x66000000),
                shape = RoundedCornerShape(8.dp)
            )
            .padding(horizontal = 12.dp, vertical = 6.dp),
        color = Color.White,
        style = MaterialTheme.typography.bodyMedium
    )
}

@Composable
fun rememberEngineApi(): EngineAPI {
    val engine = remember { EngineAPI() }

    DisposableEffect(engine) {
        engine.setupForGestures()
        onDispose {
            engine.destroy()
        }
    }

    return engine
}

@Composable
fun rememberSceneRenderer(
    scene: Scene,
    fpsSampler: Long = 1000L
): SceneRenderState {
    val engine = rememberEngineApi()
    val fpsState = remember { rememberFpsState() }
    val handleMap = remember(engine) { mutableStateMapOf<String, EngineModelHandle>() }
    val trackedModels = remember(engine) { mutableMapOf<String, TrackedModel>() }
    val rotationJobs = remember(engine) { mutableMapOf<String, Job>() }
    val modelUpdateJobs = remember(engine) { mutableMapOf<String, ModelUpdateJob>() }
    val updateScope = remember(engine) { SceneUpdateScope(handleMap) }
    val frameUpdates = remember(engine) {
        MutableSharedFlow<Unit>(
            extraBufferCapacity = 1,
            onBufferOverflow = BufferOverflow.DROP_OLDEST
        )
    }

    LaunchedEffect(engine, scene.camera.position) {
        val position = scene.camera.position
        engine.setCameraPosition(position.x, position.y, position.z)
    }

    LaunchedEffect(engine, scene.camera.rotation) {
        val rotation = scene.camera.rotation
        engine.setCameraRotation(rotation.x, rotation.y, rotation.z)
    }

    LaunchedEffect(engine, scene.models) {
        val fullRotation = (PI * 2f).toFloat()
        val desiredModels = scene.models

        val staleIds = trackedModels.keys - desiredModels.keys
        staleIds.forEach { id ->
            rotationJobs.remove(id)?.cancel()
            modelUpdateJobs.remove(id)?.job?.cancel()
            trackedModels.remove(id)
            handleMap.remove(id)
        }

        desiredModels.forEach { (id, model) ->
            val tracked = trackedModels[id]
            val needsReload = tracked == null || tracked.model.assetPath != model.assetPath
            val handle = if (needsReload) {
                val translation = model.translation
                val loadedId = engine.loadModel(
                    model.assetPath,
                    translation.x,
                    translation.y,
                    translation.z,
                    model.scale
                )
                EngineModelHandle(
                    id = loadedId,
                    assetPath = model.assetPath,
                    engine = engine
                ).also { newHandle ->
                    handleMap[id] = newHandle
                }
            } else {
                tracked.handle.also { existing ->
                    handleMap[id] = existing
                }
            }

            val previousModel = tracked?.model

            if (previousModel == null || previousModel.translation != model.translation) {
                val translation = model.translation
                handle.translateTo(translation.x, translation.y, translation.z)
            }

            if (previousModel == null || previousModel.scale != model.scale) {
                handle.scaleTo(model.scale)
            }

            if (previousModel == null || previousModel.rotation != model.rotation) {
                val rotation = model.rotation
                handle.rotateTo(rotation.x, rotation.y, rotation.z)
            }

            val autoRotate = model.autoRotate
            if (autoRotate != null) {
                val currentJob = rotationJobs[id]
                if (currentJob == null || !currentJob.isActive || previousModel?.autoRotate != autoRotate) {
                    currentJob?.cancel()
                    val speeds = autoRotate.speed
                    val hasRotation = speeds.x != 0f || speeds.y != 0f || speeds.z != 0f
                    if (hasRotation) {
                        val modelHandle = handle
                        rotationJobs[id] = launch {
                            var rotationX = model.rotation.x
                            var rotationY = model.rotation.y
                            var rotationZ = model.rotation.z
                            val modelId = modelHandle.id

                            while (isActive) {
                                rotationX = advanceRotation(rotationX, speeds.x, fullRotation)
                                rotationY = advanceRotation(rotationY, speeds.y, fullRotation)
                                rotationZ = advanceRotation(rotationZ, speeds.z, fullRotation)

                                engine.rotate(modelId, rotationX, rotationY, rotationZ)
                                delay(autoRotate.intervalMs)
                            }
                        }
                    }
                }
            } else {
                rotationJobs.remove(id)?.cancel()
            }

            val updateBlock = model.onUpdate
            if (updateBlock != null) {
                val existingJob = modelUpdateJobs[id]
                if (existingJob?.block !== updateBlock || existingJob.job.isActive.not()) {
                    existingJob?.job?.cancel()
                    val modelHandle = handle
                    val job = launch {
                        while (isActive) {
                            updateBlock.invoke(modelHandle)
                            delay(MODEL_UPDATE_INTERVAL_MS)
                        }
                    }
                    modelUpdateJobs[id] = ModelUpdateJob(updateBlock, job)
                }
            } else {
                modelUpdateJobs.remove(id)?.job?.cancel()
            }

            trackedModels[id] = TrackedModel(handle, model)
        }
    }

    LaunchedEffect(engine, fpsSampler) {
        while (isActive) {
            fpsState.value = engine.getFps()
            delay(fpsSampler)
        }
    }

    return remember(engine) {
        SceneRenderState(
            engine = engine,
            fpsState = fpsState,
            modelHandles = handleMap,
            frameUpdates = frameUpdates
        )
    }
}

private data class TrackedModel(
    val handle: EngineModelHandle,
    val model: SceneModel
)

private data class ModelUpdateJob(
    val block: EngineModelHandle.() -> Unit,
    val job: Job
)

private fun advanceRotation(current: Float, delta: Float, fullRotation: Float): Float {
    if (delta == 0f) {
        return current
    }

    var result = current + delta
    if (result > fullRotation) {
        result -= fullRotation
    } else if (result < -fullRotation) {
        result += fullRotation
    }

    return result
}