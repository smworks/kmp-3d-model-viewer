package lt.smworks.multiplatform3dengine.vulkan

import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.MotionEvent
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.viewinterop.AndroidView
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlin.math.abs
import kotlin.math.PI

actual typealias VulkanRenderTarget = Surface

private const val LOG_TAG = "VulkanScreen"

private var currentRenderer: EngineAPI? = null

internal fun setCurrentRendererForGestures(renderer: EngineAPI) {
    currentRenderer = renderer
}

@Composable
actual fun VulkanScreen(
    modifier: Modifier,
    engine: EngineAPI,
    onError: (error: String) -> Unit
) {
    val context = LocalContext.current
    val supported = remember { VulkanSupport.isSupported(context) }
    if (supported) {
        AndroidView(
            modifier = modifier,
            factory = { viewContext ->
                SurfaceView(viewContext).apply {
                    var lastTouchX = 0f
                    var lastTouchY = 0f
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

                    setOnTouchListener { _, event ->
                        when (event.actionMasked) {
                            MotionEvent.ACTION_DOWN -> {
                                activePointerId = event.getPointerId(0)
                                lastTouchX = event.x
                                lastTouchY = event.y
                                true
                            }

                            MotionEvent.ACTION_POINTER_DOWN -> {
                                val index = event.actionIndex
                                activePointerId = event.getPointerId(index)
                                lastTouchX = event.getX(index)
                                lastTouchY = event.getY(index)
                                true
                            }

                            MotionEvent.ACTION_MOVE -> {
                                if (activePointerId == MotionEvent.INVALID_POINTER_ID) {
                                    return@setOnTouchListener false
                                }

                                val pointerIndex = event.findPointerIndex(activePointerId)
                                if (pointerIndex == -1) {
                                    return@setOnTouchListener false
                                }

                                val currentX = event.getX(pointerIndex)
                                val currentY = event.getY(pointerIndex)
                                val deltaX = currentX - lastTouchX
                                val deltaY = currentY - lastTouchY

                                lastTouchX = currentX
                                lastTouchY = currentY

                                val sensitivity = 0.001f

                                currentRenderer?.let { renderer ->
                                    val absDeltaX = abs(deltaX)
                                    val absDeltaY = abs(deltaY)

                                    if (absDeltaX > 1f || absDeltaY > 1f) {
                                        renderer.rotateCamera(-deltaX * sensitivity, -deltaY * sensitivity, 0f)
                                    }
                                }
                                true
                            }

                            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                                activePointerId = MotionEvent.INVALID_POINTER_ID
                                true
                            }

                            MotionEvent.ACTION_POINTER_UP -> {
                                val pointerIndex = event.actionIndex
                                val pointerId = event.getPointerId(pointerIndex)
                                if (pointerId == activePointerId) {
                                    val newIndex = if (pointerIndex == 0) 1 else 0
                                    if (newIndex < event.pointerCount) {
                                        activePointerId = event.getPointerId(newIndex)
                                        lastTouchX = event.getX(newIndex)
                                        lastTouchY = event.getY(newIndex)
                                    } else {
                                        activePointerId = MotionEvent.INVALID_POINTER_ID
                                    }
                                }
                                true
                            }

                            else -> false
                        }
                    }
                }
            }
        )
    } else {
        LaunchedEffect(Unit) {
            onError("Vulkan is not supported")
        }
    }
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
fun rememberEngineScene(
    block: EngineSceneBuilder.() -> Unit
): EngineSceneState {
    val sceneSpec = engineScene(block)
    return rememberEngineScene(sceneSpec)
}

@Composable
fun rememberEngineScene(
    sceneSpec: EngineSceneSpec
): EngineSceneState {
    val engine = rememberEngineApi()
    val fpsState = remember { rememberFpsState() }
    val loadedModels = remember(engine) { mutableStateMapOf<EngineModelHandleRef, EngineModelHandle>() }
    val rotationJobs = remember(engine) { mutableMapOf<EngineModelHandleRef, Job>() }
    val updateScope = remember(engine) { EngineSceneUpdateScope(loadedModels) }

    LaunchedEffect(engine, sceneSpec.cameraPosition) {
        val position = sceneSpec.cameraPosition
        engine.setCameraPosition(position.x, position.y, position.z)
    }

    LaunchedEffect(engine, sceneSpec.cameraRotation) {
        val rotation = sceneSpec.cameraRotation
        engine.setCameraRotation(rotation.x, rotation.y, rotation.z)
    }

    LaunchedEffect(engine, sceneSpec.models) {
        val fullRotation = (PI * 2f).toFloat()
        val activeRefs = sceneSpec.models.toSet()

        val staleRefs = rotationJobs.keys - activeRefs
        staleRefs.forEach { ref ->
            rotationJobs.remove(ref)?.cancel()
        }

        loadedModels.keys.toList()
            .filter { ref -> ref !in activeRefs }
            .forEach { ref ->
                loadedModels.remove(ref)
            }

        sceneSpec.models.forEach { ref ->
            val spec = ref.spec
            val translation = spec.translation
            val handle = loadedModels[ref] ?: run {
                val modelId = engine.loadModel(
                    spec.assetPath,
                    translation.x,
                    translation.y,
                    translation.z,
                    spec.scale
                )
                EngineModelHandle(
                    id = modelId,
                    assetPath = spec.assetPath,
                    engine = engine
                )
            }
            loadedModels[ref] = handle

            handle.translateTo(translation.x, translation.y, translation.z)
            handle.scaleTo(spec.scale)

            val autoRotate = spec.autoRotate
            if (autoRotate != null) {
                val currentJob = rotationJobs[ref]
                if (currentJob == null || !currentJob.isActive) {
                    val modelHandle = handle
                    rotationJobs[ref] = launch {
                        var rotationX = 0f
                        var rotationY = 0f
                        var rotationZ = 0f
                        val modelId = modelHandle.id

                        val hasRotation = autoRotate.speedX != 0f ||
                                autoRotate.speedY != 0f ||
                                autoRotate.speedZ != 0f

                        if (!hasRotation) return@launch

                        while (isActive) {
                            rotationX = advanceRotation(rotationX, autoRotate.speedX, fullRotation)
                            rotationY = advanceRotation(rotationY, autoRotate.speedY, fullRotation)
                            rotationZ = advanceRotation(rotationZ, autoRotate.speedZ, fullRotation)

                            engine.rotate(modelId, rotationX, rotationY, rotationZ)
                            delay(autoRotate.intervalMs)
                        }
                    }
                }
            } else {
                rotationJobs.remove(ref)?.cancel()
            }
        }
    }

    LaunchedEffect(engine, sceneSpec.fpsSamplePeriodMs) {
        while (isActive) {
            fpsState.value = engine.getFps()
            delay(sceneSpec.fpsSamplePeriodMs)
        }
    }

    LaunchedEffect(engine, sceneSpec.onUpdate) {
        val updater = sceneSpec.onUpdate ?: return@LaunchedEffect
        while (isActive) {
            updater.invoke(updateScope)
            delay(16L)
        }
    }

    return remember(engine) {
        EngineSceneState(
            engine = engine,
            fpsState = fpsState,
            modelHandles = loadedModels
        )
    }
}

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