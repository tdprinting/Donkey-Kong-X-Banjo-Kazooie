// JNI side of the Android app: ROM checks and update checks reuse the same C++ code as the
// desktop launcher (jiggy/rom_import.hpp, jiggy/updater.hpp).
#include <jni.h>

#include <string>
#include <vector>

#include "jiggy/rom_import.hpp"
#include "jiggy/updater.hpp"

namespace {

std::string to_string(JNIEnv* env, jstring s) {
    if (!s) {
        return {};
    }
    const char* chars = env->GetStringUTFChars(s, nullptr);
    std::string out = chars ? chars : "";
    if (chars) {
        env->ReleaseStringUTFChars(s, chars);
    }
    return out;
}

jobjectArray string_array(JNIEnv* env, const std::vector<std::string>& values) {
    jobjectArray out = env->NewObjectArray(jsize(values.size()), env->FindClass("java/lang/String"), nullptr);
    for (size_t i = 0; i < values.size(); i++) {
        jstring s = env->NewStringUTF(values[i].c_str());
        env->SetObjectArrayElement(out, jsize(i), s);
        env->DeleteLocalRef(s);
    }
    return out;
}

} // namespace

// String[] { "1" or "0", message } for the ROM in `rom`, stored into `dir` when it's right.
// game: 0 = Donkey Kong 64, 1 = Banjo-Kazooie.
extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_tdprinting_jiggyjungle_Native_importRom(JNIEnv* env, jclass, jbyteArray rom, jint game, jstring dir) {
    const jsize len = env->GetArrayLength(rom);
    std::vector<uint8_t> bytes(static_cast<size_t>(len));
    env->GetByteArrayRegion(rom, 0, len, reinterpret_cast<jbyte*>(bytes.data()));
    const auto expected = game == 1 ? jiggy::Game::BanjoKazooie : jiggy::Game::DonkeyKong64;
    const auto result = jiggy::import_rom(std::move(bytes), expected, to_string(env, dir));
    return string_array(env, {result.ok ? "1" : "0", result.message});
}

// The stored file name for a game, so Java checks the same place C++ writes to.
extern "C" JNIEXPORT jstring JNICALL
Java_io_github_tdprinting_jiggyjungle_Native_storedRomName(JNIEnv* env, jclass, jint game) {
    return env->NewStringUTF(
        jiggy::stored_rom_filename(game == 1 ? jiggy::Game::BanjoKazooie : jiggy::Game::DonkeyKong64));
}

// String[] { version, apk url, apk size, release page } when the latest release is newer
// than `current` and has an Android APK; null otherwise.
extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_tdprinting_jiggyjungle_Native_findUpdate(JNIEnv* env, jclass, jstring json, jstring current) {
    namespace updater = jiggy::updater;
    const auto current_version = updater::Version::parse(to_string(env, current));
    if (!current_version) {
        return nullptr;
    }
    std::string error;
    const std::string body = to_string(env, json);
    const auto update = updater::find_update(body, *current_version, updater::Platform::AndroidArm64, &error);
    if (!update) {
        return nullptr;
    }
    return string_array(env, {update->release.version.to_string(), update->asset.download_url,
                              std::to_string(update->asset.size), update->release.page_url});
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_tdprinting_jiggyjungle_Native_latestReleaseUrl(JNIEnv* env, jclass) {
    return env->NewStringUTF(jiggy::updater::kLatestReleaseUrl);
}
