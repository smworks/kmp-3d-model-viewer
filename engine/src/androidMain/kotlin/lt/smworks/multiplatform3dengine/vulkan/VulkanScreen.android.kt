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
                var lastTouchX = 0f
                var lastTouchY = 0f
                var activePointerId = MotionEvent.INVALID_POINTER_ID

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

                            val sensitivity = 0.002f // radians per pixel

                            currentRenderer?.let { renderer ->
                                val absDeltaX = abs(deltaX)
                                val absDeltaY = abs(deltaY)

                                if (absDeltaX > 1f || absDeltaY > 1f) {
                                    renderer.rotateCamera(-deltaX * sensitivity, deltaY * sensitivity, 0f)
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