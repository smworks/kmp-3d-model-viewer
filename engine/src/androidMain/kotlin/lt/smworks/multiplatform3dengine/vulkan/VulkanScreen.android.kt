package lt.smworks.multiplatform3dengine.vulkan

import android.content.Context
import android.os.Build
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.isActive

private const val LOG_TAG = "VulkanScreen"
private const val DEFAULT_REFRESH_RATE = 60f
private const val FPS_SAMPLE_PERIOD_MS = 250L

@Composable
actual fun VulkanScreen(
    modifier: Modifier,
    scene: Scene,
    onUpdate: () -> Unit,
    config: Config
) {
    val context = LocalContext.current
    val renderState = rememberSceneRenderer(scene = scene, targetFps = config.targetFps)
    val engine = renderState.engine
    val supported = remember(context) { VulkanSupport.isSupported(context) }
    DisposableEffect(engine) {
        engine.setOnFrameUpdate {
            renderState.notifyFrameRendered()
        }
        onDispose {
            engine.setOnFrameUpdate(null)
        }
    }

    LaunchedEffect(renderState.frameUpdates, onUpdate) {
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
        onDispose {
            engine.destroy()
        }
    }

    return engine
}

@Composable
fun rememberSceneRenderer(
    scene: Scene,
    targetFps: TargetFPS
): SceneRenderState {
    val context = LocalContext.current
    val engine = rememberEngineApi()
    val memory = remember(engine) {
        RendererMemory(
            fpsState = mutableIntStateOf(0),
            trackedModels = mutableMapOf(),
            frameUpdates = MutableSharedFlow(
                extraBufferCapacity = 1,
                onBufferOverflow = BufferOverflow.DROP_OLDEST
            )
        )
    }
    val renderState = remember(engine) {
        SceneRenderState(
            engine = engine,
            fpsState = memory.fpsState,
            frameUpdates = memory.frameUpdates
        )
    }
    val trackedModels = memory.trackedModels

    LaunchedEffect(engine, scene.camera.position) {
        val position = scene.camera.position
        engine.setCameraPosition(position.x, position.y, position.z)
    }

    LaunchedEffect(engine, scene.camera.rotation) {
        val rotation = scene.camera.rotation
        engine.setCameraRotation(rotation.x, rotation.y, rotation.z)
    }

    LaunchedEffect(engine, scene.models) {
        val desiredModels = scene.models

        val staleIds = trackedModels.keys - desiredModels.keys
        staleIds.forEach { id ->
            trackedModels.remove(id)
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
                )
            } else {
                tracked.handle
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

            trackedModels[id] = TrackedModel(handle, model)
        }
    }

    FramesPerSecond(engine, targetFps, context)

    LaunchedEffect(engine) {
        while (isActive) {
            memory.fpsState.value = engine.getFps()
            delay(FPS_SAMPLE_PERIOD_MS)
        }
    }

    return renderState
}

@Composable
private fun FramesPerSecond(
    engine: EngineAPI,
    targetFps: TargetFPS,
    context: Context
) {
    val framesPerSecond = remember {
        if (targetFps == TargetFPS.FPS_VSYNC) {
            context.getDisplayRefreshRate()
        } else {
            targetFps.resolveTargetFps()
        }
    }
    LaunchedEffect(engine, framesPerSecond) {
        engine.setFrameRateLimit(framesPerSecond)
    }
}

private data class TrackedModel(
    val handle: EngineModelHandle,
    val model: SceneModel
)

private class RendererMemory(
    val fpsState: MutableState<Int>,
    val trackedModels: MutableMap<String, TrackedModel>,
    val frameUpdates: MutableSharedFlow<Unit>
)

private fun Context.getDisplayRefreshRate(): Float {
    val current = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        display.refreshRate
    } else {
        @Suppress("DEPRECATION")
        (getSystemService(Context.WINDOW_SERVICE) as? WindowManager)?.defaultDisplay?.refreshRate
    }
    return current?.takeIf { it > 0f } ?: DEFAULT_REFRESH_RATE
}

private fun TargetFPS.resolveTargetFps(): Float? = when (this) {
    TargetFPS.FPS_24 -> 24f
    TargetFPS.FPS_30 -> 30f
    TargetFPS.FPS_60 -> 60f
    TargetFPS.FPS_UNLIMITED -> null
    TargetFPS.FPS_VSYNC -> null
}
