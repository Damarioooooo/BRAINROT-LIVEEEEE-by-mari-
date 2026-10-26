#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

using namespace geode::prelude;

namespace {

struct Entry {
    std::string text;
    std::string sound; // file name (preset) or absolute path (custom); empty = no sound
};

struct Preset {
    int percent;
    const char* text;
    const char* sound;
};

// Sound files go in resources/sounds/ (see the README there).
static const std::vector<Preset> PRESETS = {
    {1,  "ONE PERCENT?? SKIBIDI TOILET",        "skibidi.mp3"},
    {6,  "SIX... WAIT FOR IT...",               "huh.mp3"},
    {7,  "SEVEN. OHIO LEVEL LUCK",              "ohio.mp3"},
    {13, "UNLUCKY. FANUM TAXED",                "vine_boom.mp3"},
    {21, "BLACKJACK! ...BUST",                  "goofy.mp3"},
    {24, "24 KARAT GOLDEN AURA",                "aura.mp3"},
    {33, "TRIPLE SIGMA",                        "sigma.mp3"},
    {41, "FORTY ONE. LOCK IN",                  "sigma.mp3"},
    {42, "THE ANSWER TO EVERYTHING. SKIBIDI",   "skibidi.mp3"},
    {50, "HALFWAY. ZERO RIZZ",                  "rizz.mp3"},
    {52, "FIVE TWO?? THAT'S SUS",               "sus.mp3"},
    {66, "SO CLOSE TO SIX SEVEN",               "huh.mp3"},
    {67, "SIX SEVENNNNN",                       "six_seven.mp3"},
    {68, "OFF BY ONE. ACTUAL BRUH",             "bruh.mp3"},
    {69, "nice.",                               "vine_boom.mp3"},
    {77, "DOUBLE LUCKY SEVEN. STILL CRASHED OUT", "fahh.mp3"},
    {80, "EIGHTY?? MID",                        "bruh.mp3"},
    {87, "ANOTHER ONE... ALMOST",               "goofy.mp3"},
    {90, "NINETY. ACTUAL OHIO",                 "ohio.mp3"},
    {95, "FIVE PERCENT FROM W RIZZ",            "rizz.mp3"},
    {97, "AURA -9999",                          "aura.mp3"},
    {98, "L + RATIO",                           "fahh.mp3"},
    {99, "ONE. PERCENT. FAAAAHHH",              "fahh.mp3"},
};

static const std::vector<const char*> RANDOM_TEXTS = {
    "SKIBIDI DEATH",
    "NO RIZZ DETECTED",
    "CERTIFIED BRUH MOMENT",
    "YOU GOT FANUM TAXED",
    "OHIO GRAVITY",
    "AURA LOST",
    "SIGMA GRINDSET BROKEN",
    "LOCK IN BRO",
    "THAT WAS SUS",
    "GYATT... THAT HURT",
    "MEWING WON'T SAVE YOU",
    "SKILL ISSUE (OHIO EDITION)",
};

static const std::vector<const char*> RANDOM_SOUNDS = {
    "vine_boom.mp3", "bruh.mp3", "fahh.mp3", "sus.mp3", "skibidi.mp3",
    "sigma.mp3", "rizz.mp3", "ohio.mp3", "aura.mp3", "goofy.mp3", "huh.mp3",
};

std::mt19937& rng() {
    static std::mt19937 gen{std::random_device{}()};
    return gen;
}

template <class T>
const T& pick(const std::vector<T>& v) {
    std::uniform_int_distribution<size_t> d(0, v.size() - 1);
    return v[d(rng())];
}

FMOD::Channel* g_channel = nullptr;
std::unordered_map<std::string, FMOD::Sound*> g_sounds;

bool soundPlaying() {
    if (!g_channel) return false;
    bool playing = false;
    if (g_channel->isPlaying(&playing) != FMOD_OK) {
        g_channel = nullptr;
        return false;
    }
    return playing;
}

std::optional<std::filesystem::path> findSound(std::string const& name) {
    if (name.empty()) return std::nullopt;
    std::error_code ec;

    std::filesystem::path direct(name);
    if (direct.is_absolute() && std::filesystem::exists(direct, ec)) return direct;

    // User-provided override: <save dir>/sounds/<name>
    auto user = Mod::get()->getSaveDir() / "sounds" / name;
    if (std::filesystem::exists(user, ec)) return user;

    // Bundled: resources/sounds/<name>
    auto bundled = Mod::get()->getResourcesDir() / "sounds" / name;
    if (std::filesystem::exists(bundled, ec)) return bundled;

    return std::nullopt;
}

void playSound(std::string const& name) {
    auto path = findSound(name);
    if (!path) {
        log::warn("Brainrot sound not found: {}", name);
        return;
    }

    auto engine = FMODAudioEngine::sharedEngine();
    if (!engine || !engine->m_system) return;
    auto sys = engine->m_system;

    auto key = path->string();
    FMOD::Sound* sound = nullptr;
    auto it = g_sounds.find(key);
    if (it != g_sounds.end()) {
        sound = it->second;
    } else {
        if (sys->createSound(key.c_str(), FMOD_DEFAULT, nullptr, &sound) != FMOD_OK || !sound) {
            log::warn("Failed to load sound: {}", key);
            return;
        }
        g_sounds[key] = sound;
    }

    FMOD::Channel* channel = nullptr;
    if (sys->playSound(sound, nullptr, false, &channel) != FMOD_OK || !channel) return;

    float vol = static_cast<float>(Mod::get()->getSettingValue<int64_t>("volume")) / 100.f;
    channel->setVolume(vol);
    g_channel = channel;
}

std::optional<Entry> entryForPercent(int percent) {
    auto mod = Mod::get();

    // Custom slots win over presets.
    for (int i = 1; i <= 3; i++) {
        auto n = std::to_string(i);
        int p = static_cast<int>(mod->getSettingValue<int64_t>("custom-percent-" + n));
        if (p > 0 && p == percent) {
            Entry e;
            e.text = mod->getSettingValue<std::string>("custom-text-" + n);
            auto file = mod->getSettingValue<std::filesystem::path>("custom-sound-" + n);
            e.sound = file.empty() ? "" : file.string();
            if (e.text.empty()) e.text = fmt::format("{}%", percent);
            return e;
        }
    }

    if (mod->getSettingValue<bool>("use-presets")) {
        for (auto const& p : PRESETS) {
            if (p.percent == percent) return Entry{p.text, p.sound};
        }
    }

    Entry e;
    bool any = false;
    if (mod->getSettingValue<bool>("random-for-others")) {
        e.text = pick(RANDOM_TEXTS);
        any = true;
    }
    if (mod->getSettingValue<bool>("random-sound-for-others")) {
        e.sound = pick(RANDOM_SOUNDS);
        any = true;
    }
    if (any) return e;
    return std::nullopt;
}

bool shouldRun(PlayLayer* pl) {
    if (!Mod::get()->getSettingValue<bool>("enabled")) return false;
    if (Mod::get()->getSettingValue<bool>("normal-mode-only") && pl->m_isPracticeMode) return false;
    return true;
}

CCLabelBMFont* findNewBestLabel(CCNode* node) {
    if (!node) return nullptr;
    if (auto label = typeinfo_cast<CCLabelBMFont*>(node)) {
        std::string s = label->getString();
        if (s.find("New Best") != std::string::npos) return label;
    }
    if (auto children = node->getChildren()) {
        for (auto child : CCArrayExt<CCNode*>(children)) {
            if (auto found = findNewBestLabel(child)) return found;
        }
    }
    return nullptr;
}

} // namespace

class $modify(BrainrotPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        bool wasDead = m_player1 && m_player1->m_isDead;
        PlayLayer::destroyPlayer(player, obj);

        if (wasDead || !m_player1 || !m_player1->m_isDead) return; // noclip or already dying
        if (!shouldRun(this)) return;
        if (!Mod::get()->getSettingValue<bool>("sounds-enabled")) return;

        auto entry = entryForPercent(this->getCurrentPercentInt());
        if (entry && !entry->sound.empty()) playSound(entry->sound);
    }

    void showNewBest(bool newReward, int orbs, int diamonds, bool demonKey, bool noRetry, bool noTitle) {
        PlayLayer::showNewBest(newReward, orbs, diamonds, demonKey, noRetry, noTitle);

        if (!shouldRun(this)) return;
        auto entry = entryForPercent(this->getCurrentPercentInt());
        if (!entry || entry->text.empty()) return;

        if (auto label = findNewBestLabel(this)) {
            label->setString(entry->text.c_str());
        }
    }

    void delayedResetLevel() {
        if (Mod::get()->getSettingValue<bool>("wait-for-sound") && soundPlaying()) {
            this->runAction(CCSequence::create(
                CCDelayTime::create(0.05f),
                CallFuncExt::create([this] { this->delayedResetLevel(); }),
                nullptr
            ));
            return;
        }
        PlayLayer::delayedResetLevel();
    }
};
