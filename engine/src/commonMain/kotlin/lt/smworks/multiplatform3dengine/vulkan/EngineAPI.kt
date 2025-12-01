package lt.smworks.multiplatform3dengine.vulkan

expect class EngineAPI() {
    fun loadModel(modelName: String, x: Float, y: Float, z: Float, scale: Float = 1.0f): Long
    fun moveCamera(delta: Float)
    fun setCameraPosition(x: Float, y: Float, z: Float)
    fun setCameraRotation(x: Float, y: Float, z: Float)
    fun rotate(modelId: Long, rotationX: Float, rotationY: Float, rotationZ: Float)
    fun rotateBy(modelId: Long, deltaX: Float, deltaY: Float, deltaZ: Float)
    fun translate(modelId: Long, x: Float, y: Float, z: Float)
    fun translateBy(modelId: Long, deltaX: Float, deltaY: Float, deltaZ: Float)
    fun scale(modelId: Long, scale: Float)
    fun scaleBy(modelId: Long, delta: Float)
    fun getFps(): Int
    fun setOnFrameUpdate(callback: (() -> Unit)?)
    fun setFrameRateLimit(fps: Float?)

    companion object {
        fun loadFile(path: String): ByteArray?
    }
}


