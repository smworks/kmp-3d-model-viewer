package lt.smworks.multiplatform3dengine

class JsPlatform : Platform {
	override val name: String = "Web with Kotlin/JS"
}

actual fun getPlatform(): Platform = JsPlatform()


