package lt.smworks.multiplatform3dengine.vulkan

import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView

actual typealias VulkanRenderTarget = Surface

@Composable
actual fun VulkanScreen(
	modifier: Modifier,
	onCreate: (VulkanRenderTarget) -> Unit,
	onDestroy: () -> Unit
) {
	AndroidView(
		modifier = modifier,
		factory = { context ->
			SurfaceView(context).apply {
				holder.addCallback(object : SurfaceHolder.Callback {
					override fun surfaceCreated(holder: SurfaceHolder) {
						val surface = holder.surface
						if (surface != null && surface.isValid) {
							onCreate(surface)
						}
					}
					override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
					}
					override fun surfaceDestroyed(holder: SurfaceHolder) {
						onDestroy()
					}
				})
			}
		}
	)
}


