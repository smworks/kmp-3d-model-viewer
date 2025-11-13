package lt.smworks.multiplatform3dengine.vulkan

expect class EngineAPI() {
    fun loadModel(x: Float, y: Float, z: Float)
    fun moveCamera(delta: Float)
}


