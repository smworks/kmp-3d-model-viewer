package lt.smworks.multiplatform3dengine.vulkan

expect class EngineAPI() {
    fun loadModel(modelName: String, x: Float, y: Float, z: Float, scale: Float)
    fun moveCamera(delta: Float)
}


