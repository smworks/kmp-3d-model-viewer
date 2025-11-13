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
private var currentRenderer: VulkanNativeRenderer? = null

// Function to set the current renderer (called from VulkanNativeRenderer)
internal fun setCurrentRendererForGestures(renderer: VulkanNativeRenderer) {
	currentRenderer = renderer
}

@Composable
actual fun VulkanScreen(
	modifier: Modifier
) {
    val renderer = remember { VulkanNativeRenderer() }
    renderer.setupForGestures()

    DisposableEffect(Unit) {
        onDispose {
            renderer.destroy()
        }
    }

	AndroidView(
		modifier = modifier,
		factory = { context ->
			var initialX = 0f
			var initialY = 0f
			
			SurfaceView(context).apply {
				holder.addCallback(object : SurfaceHolder.Callback {
					override fun surfaceCreated(holder: SurfaceHolder) {
						val surface = holder.surface
						if (surface != null && surface.isValid) {
                            renderer.init(surface, context.assets)
                            renderer.start()
						}
					}
					override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
					}
					override fun surfaceDestroyed(holder: SurfaceHolder) {
                        renderer.destroy()
					}
				})
				
				// Touch gesture detection with continuous tracking
				var lastX = 0f
				var lastY = 0f
				var isTracking = false
				
				setOnTouchListener { _, event ->
					when (event.action) {
						MotionEvent.ACTION_DOWN -> {
							initialX = event.x
							initialY = event.y
							lastX = event.x
							lastY = event.y
							isTracking = true
							true
						}
						MotionEvent.ACTION_MOVE -> {
							if (isTracking) {
								// Calculate incremental movement since last position
								val deltaX = event.x - lastX
								val deltaY = event.y - lastY
								
								// Apply rotation based on movement delta
								// Scale factor for sensitivity (adjust as needed)
								val sensitivity = 0.002f // radians per pixel
								
								currentRenderer?.let { renderer ->
									val absDeltaX = abs(deltaX)
									val absDeltaY = abs(deltaY)
									
									// Only rotate if movement is significant enough
									if (absDeltaX > 1f || absDeltaY > 1f) {
										when {
											absDeltaX > absDeltaY -> {
												// Horizontal movement - rotate around Y axis (yaw)
												renderer.rotateCamera(-deltaX * sensitivity, 0f, 0f)
											}
											else -> {
												// Vertical movement - rotate around X axis (pitch)
												renderer.rotateCamera(0f, deltaY * sensitivity, 0f)
											}
										}
									}
								}
								
								// Update last position for next move event
								lastX = event.x
								lastY = event.y
							}
							true
						}
						MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
							isTracking = false
							true
						}
						else -> false
					}
				}
			}
		}
	)
}



