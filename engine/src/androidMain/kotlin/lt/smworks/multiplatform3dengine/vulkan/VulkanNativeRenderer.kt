package lt.smworks.multiplatform3dengine.vulkan

import android.view.Surface
import java.util.concurrent.atomic.AtomicBoolean

class VulkanNativeRenderer {
	private val running = AtomicBoolean(false)
	private var thread: Thread? = null

	init {
		System.loadLibrary("vkrenderer")
	}

	fun init(surface: Surface) {
		nativeInit(surface)
	}

	fun start() {
		if (running.getAndSet(true)) return
		thread = Thread {
			while (running.get()) {
				nativeRender()
			}
		}.apply { start() }
	}

	fun stop() {
		running.set(false)
		thread?.join()
		thread = null
	}

	fun resize(width: Int, height: Int) {
		nativeResize(width, height)
	}

	fun destroy() {
		stop()
		nativeDestroy()
	}

	private external fun nativeInit(surface: Surface)
	private external fun nativeResize(width: Int, height: Int)
	private external fun nativeRender()
	private external fun nativeDestroy()
}


