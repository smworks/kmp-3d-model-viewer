package lt.smworks.multiplatform3dengine.vulkan

import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.MotionEvent
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import kotlin.math.abs

actual typealias VulkanRenderTarget = Surface

// Store the current renderer instance for gesture handling
private var currentRenderer: EngineAPI? = null

// Function to set the current renderer (called from EngineAPI)
internal fun setCurrentRendererForGestures(renderer: EngineAPI) {
    currentRenderer = renderer
}

@Composable
actual fun VulkanScreen(
    modifier: Modifier,
    engine: EngineAPI
) {
    AndroidView(
        modifier = modifier,
        factory = { context ->
            SurfaceView(context).apply {
                holder.addCallback(object : SurfaceHolder.Callback {
                    override fun surfaceCreated(holder: SurfaceHolder) {
                        val surface = holder.surface
                        if (surface != null && surface.isValid) {
                            engine.init(surface, context.assets)
                            engine.start()
                        }
                    }

                    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                    }

                    override fun surfaceDestroyed(holder: SurfaceHolder) {
                        engine.destroy()
                    }
                })

                setOnTouchListener { _, event ->
                    when (event.action) {
                        MotionEvent.ACTION_DOWN -> {
                            true
                        }

                        MotionEvent.ACTION_MOVE -> {
                            val deltaX = event.x
                            val deltaY = event.y

                            val sensitivity = 0.002f // radians per pixel

                            currentRenderer?.let { renderer ->
                                val absDeltaX = abs(deltaX)
                                val absDeltaY = abs(deltaY)

                                if (absDeltaX > 1f || absDeltaY > 1f) {
                                    when {
                                        absDeltaX > absDeltaY -> {
                                            renderer.rotateCamera(-deltaX * sensitivity, 0f, 0f)
                                        }

                                        else -> {
                                            renderer.rotateCamera(0f, deltaY * sensitivity, 0f)
                                        }
                                    }
                                }
                            }
                            true
                        }

                        MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                            true
                        }

                        else -> false
                    }
                }
            }
        }
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