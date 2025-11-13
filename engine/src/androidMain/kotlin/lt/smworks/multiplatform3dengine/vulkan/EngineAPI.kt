package lt.smworks.multiplatform3dengine.vulkan

import android.view.Surface
import android.content.res.AssetManager
import java.util.concurrent.atomic.AtomicBoolean

actual class EngineAPI actual constructor() {
    private val running = AtomicBoolean(false)
    private var thread: Thread? = null
    private var isNativeReady = false

    private data class ModelState(val x: Float, val y: Float, val z: Float)
    private val modelStates = mutableListOf<ModelState>()
    private var accumulatedCameraDistance = 0f
    private var accumulatedYaw = 0f
    private var accumulatedPitch = 0f
    private var accumulatedRoll = 0f

    init {
        System.loadLibrary("vkrenderer")
    }

    fun init(surface: Surface, assetManager: AssetManager) {
        nativeInit(surface, assetManager)
        isNativeReady = true
        restoreSceneState()
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

    fun rotateCamera(yaw: Float, pitch: Float, roll: Float) {
        accumulatedYaw += yaw
        accumulatedPitch += pitch
        accumulatedRoll += roll
        if (isNativeReady) {
            nativeRotateCamera(yaw, pitch, roll)
        }
    }

    actual fun loadModel(x: Float, y: Float, z: Float) {
        modelStates.add(ModelState(x, y, z))
        if (isNativeReady) {
            nativeLoadModel(x, y, z)
        }
    }

    actual fun moveCamera(delta: Float) {
        accumulatedCameraDistance += delta
        if (isNativeReady) {
            nativeMoveCamera(delta)
        }
    }

    fun setupForGestures() {
        // Store this renderer instance for gesture handling
        setCurrentRendererForGestures(this)
    }

    fun destroy() {
        stop()
        nativeDestroy()
        isNativeReady = false
    }

    private fun restoreSceneState() {
        modelStates.forEach { state ->
            nativeLoadModel(state.x, state.y, state.z)
        }
        if (accumulatedCameraDistance != 0f) {
            nativeMoveCamera(accumulatedCameraDistance)
        }
        if (accumulatedYaw != 0f || accumulatedPitch != 0f || accumulatedRoll != 0f) {
            nativeRotateCamera(accumulatedYaw, accumulatedPitch, accumulatedRoll)
        }
    }

    private external fun nativeInit(surface: Surface, assetManager: AssetManager)
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeRender()
    private external fun nativeRotateCamera(yaw: Float, pitch: Float, roll: Float)
    private external fun nativeDestroy()
    private external fun nativeLoadModel(x: Float, y: Float, z: Float)
    private external fun nativeMoveCamera(delta: Float)
}


