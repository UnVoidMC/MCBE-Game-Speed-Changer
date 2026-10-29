#include "mod/MyMod.h"

#include <atomic>
#include <cstdint>
#include <filesystem>

#include <pl/Mod.hpp>
#include <pl/memory/Hook.hpp>
#include <pl/memory/Signature.hpp>

namespace clange_me {

namespace {

constexpr const char* DIMENSION_TICK_PATTERN =
    "? ? ? D1 ? ? ? FD ? ? ? 6D ? ? ? 6D ? ? ? A9 "
    "? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? "
    "A9 ? ? ? 91 5C D0 3B D5 1F 20 03 D5";

using DimensionTickFn = void (*)(void*);

DimensionTickFn originalDimensionTick = nullptr;
void* dimensionTickTarget = nullptr;

std::atomic<std::uint64_t> tickCalls{0};

void dimensionTickDetour(void* dimension) {
    tickCalls.fetch_add(1, std::memory_order_relaxed);

    // Pass-through only: preserve Minecraft's original behaviour.
    if (originalDimensionTick) {
        originalDimensionTick(dimension);
    }
}

} // namespace

ClangeMeMod& ClangeMeMod::instance() {
    static ClangeMeMod instance;
    return instance;
}

ClangeMeMod::ClangeMeMod()
    : mSelf(*ll::mod::NativeMod::current()) {}

bool ClangeMeMod::load() {
    auto& self = getSelf();
    self.getLogger().debug("Loading...");

    std::error_code ec;

    std::filesystem::create_directories(self.getDataDir(), ec);
    if (ec) {
        self.getLogger().error(
            "Failed to create data directory {}: {}",
            self.getDataDir().string(), ec.message()
        );
        return false;
    }

    ec.clear();

    std::filesystem::create_directories(self.getConfigDir(), ec);
    if (ec) {
        self.getLogger().error(
            "Failed to create config directory {}: {}",
            self.getConfigDir().string(), ec.message()
        );
        return false;
    }

    mConfigFile.emplace();

    if (!mConfigFile->load()) {
        self.getLogger().warn("Failed to load typed config");
        return false;
    }

    mConfig = mConfigFile->value();

    self.getLogger().info(
        "Loaded {} from {}",
        self.getName(), self.getModDir().string()
    );

    return true;
}

bool ClangeMeMod::enable() {
    auto& self = getSelf();

    if (!mConfig.enabled) {
        self.getLogger().info("[TAS TEST] Disabled by config");
        return true;
    }

    self.getLogger().info("[TAS TEST] Resolving DimensionTick");

    const auto address = pl::memory::resolveSignature(
        DIMENSION_TICK_PATTERN,
        "libminecraftpe.so"
    );

    if (!address) {
        self.getLogger().warn("[TAS TEST] DimensionTick NOT FOUND");
        return true;
    }

    dimensionTickTarget = reinterpret_cast<void*>(address);
    tickCalls.store(0, std::memory_order_relaxed);

    self.getLogger().info("[TAS TEST] Installing pass-through hook");

    const int result = pl::memory::hook(
        dimensionTickTarget,
        reinterpret_cast<void*>(dimensionTickDetour),
        reinterpret_cast<void**>(&originalDimensionTick)
    );

    if (result != 0) {
        self.getLogger().error(
            "[TAS TEST] Hook installation failed: {}", result
        );
        dimensionTickTarget = nullptr;
        originalDimensionTick = nullptr;
        return true;
    }

    self.getLogger().info(
        "[TAS TEST] DimensionTick pass-through hook installed"
    );

    return true;
}

bool ClangeMeMod::disable() {
    auto& logger = getSelf().getLogger();

    if (dimensionTickTarget) {
        logger.info("[TAS TEST] Removing DimensionTick hook");

        const bool removed = pl::memory::unhook(
            dimensionTickTarget,
            reinterpret_cast<void*>(dimensionTickDetour)
        );

        logger.info(
            "[TAS TEST] Hook removal: {}",
            removed ? "SUCCESS" : "FAILED"
        );

        if (removed) {
            dimensionTickTarget = nullptr;
            originalDimensionTick = nullptr;
        }
    }

    logger.info(
        "[TAS TEST] DimensionTick calls observed: {}",
        tickCalls.load(std::memory_order_relaxed)
    );

    return true;
}

bool ClangeMeMod::unload() {
    if (dimensionTickTarget) {
        getSelf().getLogger().error(
            "[TAS TEST] Cannot safely unload: hook still installed"
        );
        return false;
    }

    mConfigFile.reset();
    return true;
}

} // namespace clange_me
