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
    @Volatile
    private var currentFps = 0
    private var frameCounter = 0L
    private var lastFpsTimestamp = 0L
    @Volatile
    private var pendingResizeWidth = 0
    @Volatile
    private var pendingResizeHeight = 0
    @Volatile
    private var hasPendingResize = false
    @Volatile
    private var frameUpdateCallback: (() -> Unit)? = null

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
    private var cameraPosX = 0f
    private var cameraPosY = 0f
    private var cameraPosZ = 4f
    private var cameraYaw = 0f
    private var cameraPitch = 0f
    private var cameraRoll = 0f

    init {
        System.loadLibrary("vkrenderer")
    }

    fun init(surface: Surface, assetManager: AssetManager) {
        setSharedAssetManager(assetManager)
        nativeInit(surface, assetManager)
        isNativeReady = true
        restoreSceneState()
        applyPendingResize()
    }

    fun start() {
        if (running.getAndSet(true)) return
        thread = Thread {
            while (running.get()) {
                nativeRender()
                frameUpdateCallback?.invoke()
                recordFrame()
            }
        }.apply { start() }
    }

    fun stop() {
        running.set(false)
        thread?.join()
        thread = null
        frameCounter = 0L
        lastFpsTimestamp = 0L
        currentFps = 0
    }

    fun resize(width: Int, height: Int) {
        if (width <= 0 || height <= 0) {
            return
        }
        if (isNativeReady) {
            nativeResize(width, height)
        } else {
            pendingResizeWidth = width
            pendingResizeHeight = height
            hasPendingResize = true
        }
    }

    fun rotateCamera(yaw: Float, pitch: Float, roll: Float) {
        cameraYaw += yaw
        cameraPitch += pitch
        cameraRoll += roll
        if (isNativeReady) {
            nativeRotateCamera(yaw, pitch, roll)
        }
    }

    actual fun getFps(): Int = currentFps
    actual fun setOnFrameUpdate(callback: (() -> Unit)?) {
        frameUpdateCallback = callback
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
        cameraPosZ -= delta
        if (isNativeReady) {
            nativeMoveCamera(delta)
        }
    }

    actual fun setCameraPosition(x: Float, y: Float, z: Float) {
        cameraPosX = x
        cameraPosY = y
        cameraPosZ = z
        if (isNativeReady) {
            nativeSetCameraPosition(x, y, z)
        }
    }

    actual fun setCameraRotation(x: Float, y: Float, z: Float) {
        cameraYaw = x
        cameraPitch = y
        cameraRoll = z
        if (isNativeReady) {
            nativeSetCameraRotation(x, y, z)
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

    actual fun rotateBy(modelId: Long, deltaX: Float, deltaY: Float, deltaZ: Float) {
        val state = modelStates[modelId] ?: return
        rotate(modelId, state.rotX + deltaX, state.rotY + deltaY, state.rotZ + deltaZ)
    }

    actual fun translate(modelId: Long, x: Float, y: Float, z: Float) {
        val state = modelStates[modelId] ?: return
        state.x = x
        state.y = y
        state.z = z
        if (isNativeReady) {
            nativeTranslateModel(modelId, x, y, z)
        }
    }

    actual fun translateBy(modelId: Long, deltaX: Float, deltaY: Float, deltaZ: Float) {
        val state = modelStates[modelId] ?: return
        translate(modelId, state.x + deltaX, state.y + deltaY, state.z + deltaZ)
    }

    actual fun scale(modelId: Long, scale: Float) {
        val state = modelStates[modelId] ?: return
        state.scale = scale
        if (isNativeReady) {
            nativeScaleModel(modelId, scale)
        }
    }

    actual fun scaleBy(modelId: Long, delta: Float) {
        val state = modelStates[modelId] ?: return
        scale(modelId, state.scale + delta)
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
        hasPendingResize = false
        frameUpdateCallback = null
    }

    private fun recordFrame() {
        val now = System.nanoTime()
        if (lastFpsTimestamp == 0L) {
            lastFpsTimestamp = now
            frameCounter = 0L
            return
        }

        frameCounter += 1
        val elapsed = now - lastFpsTimestamp
        if (elapsed >= 1_000_000_000L) {
            val frames = frameCounter
            currentFps = if (elapsed > 0L) {
                ((frames * 1_000_000_000L) / elapsed).toInt()
            } else {
                frames.toInt()
            }
            frameCounter = 0L
            lastFpsTimestamp = now
        }
    }

    private fun restoreSceneState() {
        modelStates.values.forEach { state ->
            nativeLoadModel(state.id, state.name, state.x, state.y, state.z, state.scale)
            if (state.rotX != 0f || state.rotY != 0f || state.rotZ != 0f) {
                nativeRotateModel(state.id, state.rotX, state.rotY, state.rotZ)
            }
        }
        nativeSetCameraPosition(cameraPosX, cameraPosY, cameraPosZ)
        nativeSetCameraRotation(cameraYaw, cameraPitch, cameraRoll)
    }

    private external fun nativeInit(surface: Surface, assetManager: AssetManager)
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeRender()
    private external fun nativeSetCameraPosition(x: Float, y: Float, z: Float)
    private external fun nativeSetCameraRotation(yaw: Float, pitch: Float, roll: Float)
    private external fun nativeRotateCamera(yaw: Float, pitch: Float, roll: Float)
    private external fun nativeDestroy()
    private external fun nativeLoadModel(modelId: Long, modelName: String, x: Float, y: Float, z: Float, scale: Float)
    private external fun nativeMoveCamera(delta: Float)
    private external fun nativeRotateModel(modelId: Long, rotationX: Float, rotationY: Float, rotationZ: Float)
    private external fun nativeTranslateModel(modelId: Long, x: Float, y: Float, z: Float)
    private external fun nativeScaleModel(modelId: Long, scale: Float)

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

    private fun applyPendingResize() {
        if (!hasPendingResize) {
            return
        }
        val width = pendingResizeWidth
        val height = pendingResizeHeight
        if (width > 0 && height > 0 && isNativeReady) {
            nativeResize(width, height)
        }
        hasPendingResize = false
    }
}


