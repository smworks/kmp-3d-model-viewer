package lt.smworks.multiplatform3dengine.vulkan

import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import platform.Foundation.NSBundle
import platform.Foundation.NSData
import platform.Foundation.dataWithContentsOfFile
import platform.posix.memcpy

actual class EngineAPI actual constructor() {
    private var nextModelId = 1L

    actual fun loadModel(modelName: String, x: Float, y: Float, z: Float, scale: Float): Long {
        // No-op on iOS until native renderer is implemented.
        return nextModelId++
    }

    actual fun moveCamera(delta: Float) {
        // No-op on iOS until native renderer is implemented.
    }

    actual fun setCameraPosition(x: Float, y: Float, z: Float) {
        // No-op on iOS until native renderer is implemented.
    }

    actual fun setCameraRotation(x: Float, y: Float, z: Float) {
        // No-op on iOS until native renderer is implemented.
    }

    actual fun rotate(modelId: Long, rotationX: Float, rotationY: Float, rotationZ: Float) {
        // No-op on iOS until native renderer is implemented.
    }

    actual fun rotateBy(modelId: Long, deltaX: Float, deltaY: Float, deltaZ: Float) {
        // No-op on iOS until native renderer is implemented.
    }

    actual fun translate(modelId: Long, x: Float, y: Float, z: Float) {
        // No-op on iOS until native renderer is implemented.
    }

    actual fun translateBy(modelId: Long, deltaX: Float, deltaY: Float, deltaZ: Float) {
        // No-op on iOS until native renderer is implemented.
    }

    actual fun scale(modelId: Long, scale: Float) {
        // No-op on iOS until native renderer is implemented.
    }

    actual fun scaleBy(modelId: Long, delta: Float) {
        // No-op on iOS until native renderer is implemented.
    }

    actual fun getFps(): Int = 0

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


