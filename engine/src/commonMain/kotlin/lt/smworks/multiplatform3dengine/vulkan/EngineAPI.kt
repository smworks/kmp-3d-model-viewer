package lt.smworks.multiplatform3dengine.vulkan

expect class EngineAPI() {
    fun loadModel(modelName: String, x: Float, y: Float, z: Float, scale: Float = 1.0f): Long
    fun moveCamera(delta: Float)
    fun rotate(modelId: Long, rotationX: Float, rotationY: Float, rotationZ: Float)
}


