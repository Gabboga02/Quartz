#include <qz/gfx/context.hpp>
#include <qz/gfx/window.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstring>
#include <array>

namespace qz::gfx {
    // Queries for instance extension availability from requested extensions and reports an error if one is not present.
    qz_nodiscard static bool query_instance_extension_availability(const std::vector<const char*>& extensions) noexcept {
        // Query for all available extensions.
        std::uint32_t count;
        qz_vulkan_check(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
        std::vector<VkExtensionProperties> available(count);
        qz_vulkan_check(vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data()));

        // Check all of the requested extensions are available.
        return std::all_of(extensions.begin(), extensions.end(), [&available](const char* name) noexcept {
            if (!std::any_of(available.begin(), available.end(), [name](const auto& current) noexcept {
                return std::strcmp(name, current.extensionName) == 0;
            })) {
                std::printf("Requested extension not found: %s\n", name);
                return false;
            }
            return true;
        });
    }

    // Queries for layer availability from requested layers and reports an error if one is not present.
    qz_nodiscard static bool query_layer_availability(const std::vector<const char*>& layers) noexcept {
        // Query for all available layers.
        std::uint32_t count;
        qz_vulkan_check(vkEnumerateInstanceLayerProperties(&count, nullptr));
        std::vector<VkLayerProperties> available(count);
        qz_vulkan_check(vkEnumerateInstanceLayerProperties(&count, available.data()));

        // Check all of the requested layers are available.
        return std::all_of(layers.begin(), layers.end(), [&available](const char* name) noexcept {
            if (!std::any_of(available.begin(), available.end(), [name](const auto& current) noexcept {
                return std::strcmp(name, current.layerName) == 0;
            })) {
                std::printf("Requested layer not found: %s\n", name);
                return false;
            }
            return true;
        });
    }

    // Queries for device extension availability from requested extensions and reports an error if one is not present.
    qz_nodiscard static bool query_device_extension_availability(VkPhysicalDevice gpu, const std::vector<const char*>& extensions) noexcept {
        // Query for all available extensions.
        std::uint32_t count;
        qz_vulkan_check(vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, nullptr));
        std::vector<VkExtensionProperties> available(count);
        qz_vulkan_check(vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, available.data()));

        // Check all of the requested extensions are available.
        return std::all_of(extensions.begin(), extensions.end(), [&available](const char* name) noexcept {
            if (!std::any_of(available.begin(), available.end(), [name](const auto& current) noexcept {
                return std::strcmp(name, current.extensionName) == 0;
            })) {
                std::printf("Requested extension not found: %s\n", name);
                return false;
            }
            return true;
        });
    }

    qz_nodiscard static bool is_graphics_card_suitable(VkPhysicalDevice gpu) noexcept {
        // Query for card's available properties and features
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(gpu, &properties);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(gpu, &features);

        // Device is suitable if it's a discrete graphics card
        return properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    }

    qz_nodiscard Context Context::create(const Settings& settings) noexcept {
        Context context{};

        // Create instance.
        VkApplicationInfo application_info{};
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pApplicationName = "QuartzVk";
        application_info.applicationVersion = settings.version;
        application_info.pEngineName = "QuartzVk";
        application_info.engineVersion = settings.version;
        application_info.apiVersion = settings.version;

        qz_assert(glfwInit(), "GLFW failed to initialize, or was not initialized correctly");
        std::uint32_t required_count;
        const char** required_extensions = glfwGetRequiredInstanceExtensions(&required_count);

        std::vector<const char*> instance_layers;
        std::vector<const char*> instance_extensions(required_extensions, required_extensions + required_count);
#if defined(QUARTZ_DEBUG)
        instance_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        instance_layers.emplace_back("VK_LAYER_KHRONOS_validation");
#endif

        // Query for enabled layers and extensions availability
        qz_assert(query_instance_extension_availability(instance_extensions),
                  "One or more instance extensions were requested, but are not available");
        qz_assert(query_layer_availability(instance_layers),
                  "One or more instance layers were requested, but are not available");

        VkInstanceCreateInfo instance_create_info{};
        instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_create_info.pApplicationInfo = &application_info;
        instance_create_info.enabledLayerCount = instance_layers.size();
        instance_create_info.ppEnabledLayerNames = instance_layers.data();
        instance_create_info.enabledExtensionCount = instance_extensions.size();
        instance_create_info.ppEnabledExtensionNames = instance_extensions.data();
        qz_vulkan_check(vkCreateInstance(&instance_create_info, nullptr, &context.instance));

        // Create validation callback (only in debug mode)
#if defined(QUARTZ_DEBUG)
        VkDebugUtilsMessengerCreateInfoEXT debug_callback_create_info{};
        debug_callback_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_callback_create_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_callback_create_info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_callback_create_info.pfnUserCallback = [](
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void*) -> VkBool32 {

            const auto severity_string = [severity]() noexcept -> const char* {
                switch (severity) {
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return "Verbose";
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    return "Info";
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return "Warning";
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   return "Error";
                }
                return "Unknown";
            }();

            const auto type_string = [type]() noexcept -> const char* {
                switch (type) {
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:     return "General";
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:  return "Validation";
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: return "Performance";
                }
                return "Unknown";
            }();

            // TODO: Use a proper logging interface.
            std::printf(
                "Vulkan Validation Message:\n"
                "    Severity: %s\n"
                "    Type:     %s\n"
                "    Message:  %s\n",
                severity_string,
                type_string,
                data->pMessage);

            if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                std::abort();
            }

            return 0;
        };

        qz_load_instance_function(context, vkCreateDebugUtilsMessengerEXT);
        qz_vulkan_check(vkCreateDebugUtilsMessengerEXT(context.instance, &debug_callback_create_info, nullptr, &context.validation));
#endif

        // Pick GPU.
        // Query for all available graphics cards.
        std::uint32_t gpu_count;
        qz_vulkan_check(vkEnumeratePhysicalDevices(context.instance, &gpu_count, nullptr));
        std::vector<VkPhysicalDevice> graphics_cards(gpu_count);
        qz_vulkan_check(vkEnumeratePhysicalDevices(context.instance, &gpu_count, graphics_cards.data()));

        // Select graphics card
        for (const auto gpu : graphics_cards) {
            if (is_graphics_card_suitable(gpu)) {
                context.gpu = gpu;
                break;
            }
        }
        qz_assert(context.gpu, "Failed to find a suitable graphics gard");

        // Pick queue family.
        // Query for all available queue families.
        std::uint32_t families_count;
        vkGetPhysicalDeviceQueueFamilyProperties(context.gpu, &families_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_properties(families_count);
        vkGetPhysicalDeviceQueueFamilyProperties(context.gpu, &families_count, queue_properties.data());

        // Select family based on capabilities (graphics-capable queue)
        for (std::uint32_t index = 0; const auto& family : queue_properties) {
            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                context.family = index;
                break;
            }
            index++;
        }

        qz_assert(context.family != -1u, "No suitable queue family found");

        // Create logical device.
        constexpr std::array priorities = { 1.0f, 0.9f };
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = context.family;
        queue_create_info.queueCount = priorities.size();
        queue_create_info.pQueuePriorities = priorities.data();

        const std::vector<const char*> enabled_extensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        qz_assert(query_device_extension_availability(context.gpu, enabled_extensions),
                  "One or more required device extensions are not available");

        VkDeviceCreateInfo device_create_info{};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.queueCreateInfoCount = 1;
        device_create_info.pQueueCreateInfos = &queue_create_info;
        device_create_info.enabledLayerCount = 0;
        device_create_info.ppEnabledLayerNames = nullptr;
        device_create_info.enabledExtensionCount = enabled_extensions.size();
        device_create_info.ppEnabledExtensionNames = enabled_extensions.data();
        device_create_info.pEnabledFeatures = nullptr;
        qz_vulkan_check(vkCreateDevice(context.gpu, &device_create_info, nullptr, &context.device));

        // Create VmaAllocator.
        VmaAllocatorCreateInfo allocator_create_info{};
        allocator_create_info.flags = {};
        allocator_create_info.instance = context.instance;
        allocator_create_info.device = context.device;
        allocator_create_info.physicalDevice = context.gpu;
        allocator_create_info.pHeapSizeLimit = nullptr;
        allocator_create_info.pRecordSettings = nullptr;
        allocator_create_info.pAllocationCallbacks = nullptr;
        allocator_create_info.pDeviceMemoryCallbacks = nullptr;
        allocator_create_info.frameInUseCount = 1;
        allocator_create_info.preferredLargeHeapBlockSize = 0;
        allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_2;
        qz_vulkan_check(vmaCreateAllocator(&allocator_create_info, &context.allocator));

        // And retrieve queues.
        vkGetDeviceQueue(context.device, context.family, 0, &context.graphics);
        vkGetDeviceQueue(context.device, context.family, 1, &context.transfer);

        // Create main command pool, used for allocating rendering command buffers.
        VkCommandPoolCreateInfo command_pool_create_info{};
        command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_create_info.queueFamilyIndex = context.family;
        qz_vulkan_check(vkCreateCommandPool(context.device, &command_pool_create_info, nullptr, &context.main_pool));

        return context;
    }

    void Context::destroy(Context& context) noexcept {
        vkDestroyCommandPool(context.device, context.main_pool, nullptr);
        vmaDestroyAllocator(context.allocator);
        vkDestroyDevice(context.device, nullptr);
#if defined(QUARTZ_DEBUG)
        qz_load_instance_function(context, vkDestroyDebugUtilsMessengerEXT);
        vkDestroyDebugUtilsMessengerEXT(context.instance, context.validation, nullptr);
#endif
        vkDestroyInstance(context.instance, nullptr);
    }
} // namespace qz::gfx