package lt.smworks.multiplatform3dengine.vulkan

actual class EngineAPI actual constructor() {
    private var nextModelId = 1L

    actual fun loadModel(modelName: String, x: Float, y: Float, z: Float, scale: Float): Long {
        // No-op on iOS until native renderer is implemented.
        return nextModelId++
    }

    actual fun moveCamera(delta: Float) {
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
}


