package lt.smworks.multiplatform3dengine

interface Platform {
	val name: String
}

expect fun getPlatform(): Platform


