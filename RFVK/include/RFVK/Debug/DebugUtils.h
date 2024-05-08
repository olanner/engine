
#pragma once

inline std::function<decltype( vkSetDebugUtilsObjectNameEXT )>		vkSetDebugUtilsObjectName;
inline std::function<decltype( vkDestroyDebugUtilsMessengerEXT )>	vkDestroyDebugUtilsMessenger;

inline bool gUseDebugLayers = false;

inline void
DebugSetObjectName( 
	const char*		name, 
	void*			object, 
	VkObjectType	type, 
	VkDevice		device )
{
	if ( gUseDebugLayers )
	{
		if ( !object )
		{
			return;
		}
		VkDebugUtilsObjectNameInfoEXT nameInfo;
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.pNext = nullptr;

		nameInfo.objectType = type;
		nameInfo.objectHandle = (uint64_t) object;
		nameInfo.pObjectName = name;

		vkSetDebugUtilsObjectName( device, &nameInfo );
	}
}