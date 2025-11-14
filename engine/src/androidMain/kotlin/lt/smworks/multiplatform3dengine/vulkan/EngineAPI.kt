package lt.smworks.multiplatform3dengine.vulkan

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.view.Surface
import android.content.res.AssetManager
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong

actual class EngineAPI actual constructor() {
    private val running = AtomicBoolean(false)
    private var thread: Thread? = null
    private var isNativeReady = false

    private data class ModelState(
        val id: Long,
        val name: String,
        var x: Float,
        var y: Float,
        var z: Float,
        var scale: Float,
        var rotX: Float = 0f,
        var rotY: Float = 0f,
        var rotZ: Float = 0f,
    )
    private val modelStates = LinkedHashMap<Long, ModelState>()
    private val modelIdGenerator = AtomicLong(1L)
    private var accumulatedCameraDistance = 0f
    private var accumulatedYaw = 0f
    private var accumulatedPitch = 0f
    private var accumulatedRoll = 0f

    init {
        System.loadLibrary("vkrenderer")
    }

    fun init(surface: Surface, assetManager: AssetManager) {
        setSharedAssetManager(assetManager)
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

    actual fun loadModel(modelName: String, x: Float, y: Float, z: Float, scale: Float): Long {
        val modelId = modelIdGenerator.getAndIncrement()
        val state = ModelState(modelId, modelName, x, y, z, scale)
        modelStates[modelId] = state
        if (isNativeReady) {
            nativeLoadModel(modelId, modelName, x, y, z, scale)
        }
        return modelId
    }

    actual fun moveCamera(delta: Float) {
        accumulatedCameraDistance += delta
        if (isNativeReady) {
            nativeMoveCamera(delta)
        }
    }

    actual fun rotate(modelId: Long, rotationX: Float, rotationY: Float, rotationZ: Float) {
        val state = modelStates[modelId] ?: return
        state.rotX = rotationX
        state.rotY = rotationY
        state.rotZ = rotationZ
        if (isNativeReady) {
            nativeRotateModel(modelId, rotationX, rotationY, rotationZ)
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
        setSharedAssetManager(null)
    }

    private fun restoreSceneState() {
        modelStates.values.forEach { state ->
            nativeLoadModel(state.id, state.name, state.x, state.y, state.z, state.scale)
            if (state.rotX != 0f || state.rotY != 0f || state.rotZ != 0f) {
                nativeRotateModel(state.id, state.rotX, state.rotY, state.rotZ)
            }
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
    private external fun nativeLoadModel(modelId: Long, modelName: String, x: Float, y: Float, z: Float, scale: Float)
    private external fun nativeMoveCamera(delta: Float)
    private external fun nativeRotateModel(modelId: Long, rotationX: Float, rotationY: Float, rotationZ: Float)

    companion object {
        private var sharedAssetManager: AssetManager? = null

        fun setSharedAssetManager(manager: AssetManager?) {
            sharedAssetManager = manager
        }

        @JvmStatic
        fun decodeTexture(path: String): ByteArray? {
            val manager = sharedAssetManager ?: return null
            val opts = BitmapFactory.Options().apply {
                inPreferredConfig = Bitmap.Config.ARGB_8888
            }
            val bitmap = try {
                manager.open(path).use { stream ->
                    BitmapFactory.decodeStream(stream, null, opts)
                }
            } catch (e: Exception) {
                null
            } ?: return null

            val width = bitmap.width
            val height = bitmap.height
            if (width <= 0 || height <= 0) {
                bitmap.recycle()
                return null
            }

            val pixels = IntArray(width * height)
            bitmap.getPixels(pixels, 0, width, 0, 0, width, height)
            bitmap.recycle()

            val totalBytes = width * height * 4
            val output = ByteArray(8 + totalBytes)
            val buffer = ByteBuffer.wrap(output).order(ByteOrder.LITTLE_ENDIAN)
            buffer.putInt(width)
            buffer.putInt(height)

            var offset = 8
            for (value in pixels) {
                val a = (value ushr 24) and 0xFF
                val r = (value ushr 16) and 0xFF
                val g = (value ushr 8) and 0xFF
                val b = value and 0xFF
                output[offset++] = r.toByte()
                output[offset++] = g.toByte()
                output[offset++] = b.toByte()
                output[offset++] = a.toByte()
            }

            return output
        }
    }
}


