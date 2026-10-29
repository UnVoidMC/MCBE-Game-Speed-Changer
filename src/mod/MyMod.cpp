#include "mod/MyMod.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string_view>

#include <pl/Mod.hpp>
#include <pl/memory/Signature.hpp>

namespace clange_me {

namespace {

struct SignatureTest {
    std::string_view name;
    std::string_view pattern;
};

constexpr std::array<SignatureTest, 4> signatures{{
    {
        "DimensionTick",
        "? ? ? D1 ? ? ? FD ? ? ? 6D ? ? ? 6D ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 5C D0 3B D5 1F 20 03 D5"
    },
    {
        "NormalTick",
        "? ? ? FC ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 ? ? ? D1 54 D0 3B D5 F3 03 00 AA ? ? ? F9 ? ? ? F8 ? ? ? 39"
    },
    {
        "ClientInstanceUpdate",
        "? ? ? FC ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 ? ? ? D1 59 D0 3B D5 ? ? ? 52 F5 03 02 2A"
    },
    {
        "LocalPlayerApplyTurnDelta",
        "? ? ? D1 ? ? ? FD ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 56 D0 3B D5 F3 03 00 AA F4 03 01 AA ? ? ? F9 ? ? ? F8 ? ? ? F9 ? ? ? F9"
    }
}};

void runSignatureTest(ll::mod::NativeMod& self) {
    auto& logger = self.getLogger();

    logger.info("[TAS TEST] Starting Minecraft signature scan");

    int found = 0;

    for (const auto& entry : signatures) {
        const std::uintptr_t address =
            pl::memory::resolveSignature(
                entry.pattern,
                "libminecraftpe.so"
            );

        if (address != 0) {
            ++found;
            logger.info("[TAS TEST] {}: FOUND", entry.name);
        } else {
            logger.warn("[TAS TEST] {}: NOT FOUND", entry.name);
        }
    }

    logger.info("[TAS TEST] Result: {}/4 signatures found", found);
    logger.info("[TAS TEST] No hooks installed. Game speed unchanged.");
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
            self.getDataDir().string(),
            ec.message()
        );
        return false;
    }

    ec.clear();

    std::filesystem::create_directories(self.getConfigDir(), ec);
    if (ec) {
        self.getLogger().error(
            "Failed to create config directory {}: {}",
            self.getConfigDir().string(),
            ec.message()
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
        self.getName(),
        self.getModDir().string()
    );

    return true;
}

bool ClangeMeMod::enable() {
    auto& self = getSelf();

    self.getLogger().debug("Enabling...");

    if (!mConfig.enabled) {
        self.getLogger().info("MCBE Tick Changer disabled by config");
        return true;
    }

    runSignatureTest(self);

    return true;
}

bool ClangeMeMod::disable() {
    getSelf().getLogger().debug("Disabling...");
    return true;
}

bool ClangeMeMod::unload() {
    getSelf().getLogger().debug("Unloading...");
    mConfigFile.reset();
    return true;
}

} // namespace clange_me
