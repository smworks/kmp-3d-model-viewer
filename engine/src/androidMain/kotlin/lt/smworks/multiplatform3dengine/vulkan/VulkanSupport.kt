package lt.smworks.multiplatform3dengine.vulkan

import android.content.Context
import android.content.pm.PackageManager

object VulkanSupport {
	fun isSupported(context: Context): Boolean {
		val pm = context.packageManager
		val hasLevel = pm.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_LEVEL)
		val hasVersion = pm.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_VERSION)
		return hasLevel || hasVersion
	}
}


