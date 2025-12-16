package lt.smworks.multiplatform3dengine.vulkan

import engine.EngineIosHandle
import engine.engine_ios_create
import engine.engine_ios_destroy
import engine.engine_ios_initialize
import engine.engine_ios_load_model
import engine.engine_ios_move_camera
import engine.engine_ios_render
import engine.engine_ios_rotate_camera
import engine.engine_ios_rotate_model
import engine.engine_ios_scale_model
import engine.engine_ios_set_camera_position
import engine.engine_ios_set_camera_rotation
import engine.engine_ios_shutdown
import engine.engine_ios_translate_model
import engine.engine_ios_resize
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.cstr
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.usePinned
import platform.Foundation.NSBundle
import platform.Foundation.NSData
import platform.Foundation.dataWithContentsOfFile
import platform.posix.memcpy

actual class EngineAPI actual constructor() {
    private var handle: EngineIosHandle? = null
    private var nextModelId = 1L

    private fun ensureHandle(): EngineIosHandle {
        val current = handle ?: engine_ios_create().also { handle = it }
        return current ?: error("Unable to create native engine handle")
    }

    private fun requireHandle(): EngineIosHandle? = handle

    fun initialize(width: UInt, height: UInt) {
        val native = ensureHandle()
        engine_ios_initialize(native, width, height)
    }

    fun resize(width: UInt, height: UInt) {
        val native = requireHandle() ?: return
        engine_ios_resize(native, width, height)
    }

    actual fun loadModel(modelName: String, x: Float, y: Float, z: Float, scale: Float): Long {
        val native = ensureHandle()
        val id = nextModelId++
        memScoped {
            val path = modelName.cstr
            engine_ios_load_model(native, path.ptr, id, x, y, z, scale)
        }
        return id
    }

    actual fun moveCamera(delta: Float) {
        requireHandle()?.let { engine_ios_move_camera(it, delta) }
    }

    actual fun setCameraPosition(x: Float, y: Float, z: Float) {
        requireHandle()?.let { engine_ios_set_camera_position(it, x, y, z) }
    }

    actual fun setCameraRotation(x: Float, y: Float, z: Float) {
        requireHandle()?.let { engine_ios_set_camera_rotation(it, x, y, z) }
    }

    actual fun rotate(modelId: Long, rotationX: Float, rotationY: Float, rotationZ: Float) {
        requireHandle()?.let { engine_ios_rotate_model(it, modelId, rotationX, rotationY, rotationZ) }
    }

    actual fun rotateBy(modelId: Long, deltaX: Float, deltaY: Float, deltaZ: Float) {
        val native = requireHandle() ?: return
        engine_ios_rotate_model(native, modelId, deltaX, deltaY, deltaZ)
    }

    actual fun translate(modelId: Long, x: Float, y: Float, z: Float) {
        requireHandle()?.let { engine_ios_translate_model(it, modelId, x, y, z) }
    }

    actual fun translateBy(modelId: Long, deltaX: Float, deltaY: Float, deltaZ: Float) {
        requireHandle()?.let { engine_ios_translate_model(it, modelId, deltaX, deltaY, deltaZ) }
    }

    actual fun scale(modelId: Long, scale: Float) {
        requireHandle()?.let { engine_ios_scale_model(it, modelId, scale) }
    }

    actual fun scaleBy(modelId: Long, delta: Float) {
        requireHandle()?.let { engine_ios_scale_model(it, modelId, delta) }
    }

    fun renderFrame() {
        requireHandle()?.let { engine_ios_render(it) }
    }

    fun shutdown() {
        handle?.let { engine_ios_shutdown(it) }
        handle?.let { engine_ios_destroy(it) }
        handle = null
    }

    actual fun getFps(): Int = 0

    actual fun setOnFrameUpdate(callback: (() -> Unit)?) {
        // Frame callbacks not wired for stub implementation.
    }

    actual fun setFrameRateLimit(fps: Float?) {
        // Frame pacing not implemented for iOS stub.
    }

    actual companion object {
        fun loadFile(path: String): ByteArray? {
            val resourceRoot = NSBundle.mainBundle.resourcePath ?: return null
            val fullPath = "$resourceRoot/$path"
            val data = NSData.dataWithContentsOfFile(fullPath) ?: return null
            val length = data.length.toInt()
            if (length <= 0) {
                return ByteArray(0)
            }
            val bytes = ByteArray(length)
            val source = data.bytes ?: return null
            bytes.usePinned { pinned ->
                val destination = pinned.addressOf(0)
                memcpy(destination, source, length.toULong())
            }
            return bytes
        }
    }
}

