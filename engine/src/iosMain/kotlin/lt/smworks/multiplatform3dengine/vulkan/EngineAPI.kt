package lt.smworks.multiplatform3dengine.vulkan

actual class EngineAPI actual constructor() {
    actual fun loadModel() {
        // No-op on iOS until native renderer is implemented.
    }

    actual fun moveCamera(delta: Float) {
        // No-op on iOS until native renderer is implemented.
    }
}


