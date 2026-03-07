#include <jni.h>
#include <string>
#include <vector>
#include <tuple>
#include <algorithm>
#include <android/log.h>
#include <cryptopp/base64.h>
#include "FAST.h"

#define LOG_TAG "PSI_NATIVE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)



// Helper to encode binary data to Base64 for safe JNI transfer
std::string toBase64(const std::string& input) {
    std::string encoded;
    CryptoPP::StringSource ss(input, true,
        new CryptoPP::Base64Encoder(
            new CryptoPP::StringSink(encoded),
            false // do not append newline
        )
    );
    return encoded;
}

// Helper to decode Base64 strings back to binary
std::string fromBase64(const std::string& input) {
    std::string decoded;
    CryptoPP::StringSource ss(input, true,
        new CryptoPP::Base64Decoder(
            new CryptoPP::StringSink(decoded)
        )
    );
    return decoded;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_psi_MainActivity_initializeNative(
        JNIEnv* env,
        jobject /* this */,
        jstring storagePathString) {
    // No longer strictly needed if we pass path everywhere, but we can keep it for backward compatibility if needed
    // For now, let's just make everything explicit.
}

// Forward declarations from queen.cpp
std::vector<std::tuple<std::string, std::string>> queen_process(std::vector<std::tuple<std::string, int>> &inp, const std::string &storage_path, std::vector<std::string> &encrypted_paths);
std::tuple<std::string, std::string, int> queen_search_client(std::string keyword, const std::string &storage_path);
std::vector<int> queen_post_process(int index_range, std::string res1_0, std::string res1_1, std::string res2_0, std::string res2_1);

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_psi_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_example_psi_UpdateActivity_generateTokens(
        JNIEnv* env,
        jobject /* this */,
        jstring storagePath,
        jobjectArray filePaths,
        jintArray keywords) {
    
    const char* storageChars = env->GetStringUTFChars(storagePath, nullptr);
    std::string sPath(storageChars);
    env->ReleaseStringUTFChars(storagePath, storageChars);

    int len = env->GetArrayLength(filePaths);
    jint* keywordsPtr = env->GetIntArrayElements(keywords, nullptr);
    
    std::vector<std::tuple<std::string, int>> input;
    for (int i = 0; i < len; i++) {
        jstring pathStr = (jstring)env->GetObjectArrayElement(filePaths, i);
        const char* pathChars = env->GetStringUTFChars(pathStr, nullptr);
        
        input.emplace_back(std::string(pathChars), (int)keywordsPtr[i]);
        
        env->ReleaseStringUTFChars(pathStr, pathChars);
        env->DeleteLocalRef(pathStr);
    }
    
    env->ReleaseIntArrayElements(keywords, keywordsPtr, JNI_ABORT);
    
    // Call the redirection logic in queen.cpp to get (u, e) tokens
    std::vector<std::string> encrypted_paths;
    auto result = queen_process(input, sPath, encrypted_paths);
    
    LOGI("Token generation complete. Result size: %zu, Files: %zu", result.size(), encrypted_paths.size());
    
    // Return Flattened array: tokens (u1, e1, ...) THEN encrypted file paths
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray combinedArray = env->NewObjectArray(result.size() * 2 + encrypted_paths.size(), stringClass, nullptr);
    
    // Fill tokens - WE MUST CAREFULLY MANAGE LOCAL REFERENCES HERE
    // Android JNI local ref table has a small limit (often 512).
    // We are generating 400k tokens, so we MUST use DeleteLocalRef.
    for (size_t i = 0; i < result.size(); i++) {
        std::string u_b64 = toBase64(std::get<0>(result[i]));
        std::string e_b64 = toBase64(std::get<1>(result[i]));
        
        jstring u_jstr = env->NewStringUTF(u_b64.c_str());
        jstring e_jstr = env->NewStringUTF(e_b64.c_str());
        
        env->SetObjectArrayElement(combinedArray, i * 2, u_jstr);
        env->SetObjectArrayElement(combinedArray, i * 2 + 1, e_jstr);
        
        // Critically important: delete the local references we just created
        env->DeleteLocalRef(u_jstr);
        env->DeleteLocalRef(e_jstr);
    }
    
    // Fill file paths (after tokens)
    int offset = result.size() * 2;
    for (size_t i = 0; i < encrypted_paths.size(); i++) {
        jstring p_jstr = env->NewStringUTF(encrypted_paths[i].c_str());
        env->SetObjectArrayElement(combinedArray, offset + i, p_jstr);
        env->DeleteLocalRef(p_jstr);
    }
    
    return combinedArray;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_psi_SearchActivity_decryptResultFile(
        JNIEnv* env,
        jobject /* this */,
        jstring storagePath,
        jstring encryptedPath,
        jstring decryptedPath) {
    
    const char* storageChars = env->GetStringUTFChars(storagePath, nullptr);
    std::string sPath(storageChars);
    env->ReleaseStringUTFChars(storagePath, storageChars);

    const char* encPathChars = env->GetStringUTFChars(encryptedPath, nullptr);
    const char* decPathChars = env->GetStringUTFChars(decryptedPath, nullptr);
    
    try {
        DSSE FAST_;
        FAST_.Setup(sPath);
        SecByteBlock key = FAST_.Get_Client_sk();
        
        decryptFile(key, std::string(encPathChars), std::string(decPathChars));
        
        env->ReleaseStringUTFChars(encryptedPath, encPathChars);
        env->ReleaseStringUTFChars(decryptedPath, decPathChars);
        return JNI_TRUE;
    } catch (const std::exception& e) {
        LOGI("Decryption error: %s", e.what());
        env->ReleaseStringUTFChars(encryptedPath, encPathChars);
        env->ReleaseStringUTFChars(decryptedPath, decPathChars);
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_example_psi_SearchActivity_getSearchToken(
        JNIEnv* env,
        jobject /* this */,
        jstring storagePath,
        jstring keyword) {
    
    const char* storageChars = env->GetStringUTFChars(storagePath, nullptr);
    std::string sPath(storageChars);
    env->ReleaseStringUTFChars(storagePath, storageChars);

    const char* keywordChars = env->GetStringUTFChars(keyword, nullptr);
    auto result = queen_search_client(std::string(keywordChars), sPath);
    env->ReleaseStringUTFChars(keyword, keywordChars);
    
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray tokenArray = env->NewObjectArray(3, stringClass, nullptr);
    
    std::string t_keyword_b64 = toBase64(std::get<0>(result));
    std::string st_c_b64 = toBase64(std::get<1>(result));

    env->SetObjectArrayElement(tokenArray, 0, env->NewStringUTF(t_keyword_b64.c_str()));
    env->SetObjectArrayElement(tokenArray, 1, env->NewStringUTF(st_c_b64.c_str()));
    env->SetObjectArrayElement(tokenArray, 2, env->NewStringUTF(std::to_string(std::get<2>(result)).c_str()));
    
    return tokenArray;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_example_psi_SearchActivity_performPostProcessing(
        JNIEnv* env,
        jobject /* this */,
        jstring storagePath,
        jint indexRange,
        jstring res1_0, jstring res1_1,
        jstring res2_0, jstring res2_1) {
    
    // storagePath not strictly used for post-processing currently but good for consistency
    const char* storageChars = env->GetStringUTFChars(storagePath, nullptr);
    std::string sPath(storageChars);
    env->ReleaseStringUTFChars(storagePath, storageChars);

    const char* r10_b64 = env->GetStringUTFChars(res1_0, nullptr);
    const char* r11_b64 = env->GetStringUTFChars(res1_1, nullptr);
    const char* r20_b64 = env->GetStringUTFChars(res2_0, nullptr);
    const char* r21_b64 = env->GetStringUTFChars(res2_1, nullptr);
    
    std::string r10 = (std::string(r10_b64) == "-1") ? "-1" : fromBase64(r10_b64);
    std::string r11 = (std::string(r11_b64) == "-1") ? "-1" : fromBase64(r11_b64);
    std::string r20 = (std::string(r20_b64) == "-1") ? "-1" : fromBase64(r20_b64);
    std::string r21 = (std::string(r21_b64) == "-1") ? "-1" : fromBase64(r21_b64);

    auto result = queen_post_process(indexRange, r10, r11, r20, r21);
    
    env->ReleaseStringUTFChars(res1_0, r10_b64);
    env->ReleaseStringUTFChars(res1_1, r11_b64);
    env->ReleaseStringUTFChars(res2_0, r20_b64);
    env->ReleaseStringUTFChars(res2_1, r21_b64);
    
    jintArray resultArray = env->NewIntArray(result.size());
    if (result.size() > 0) {
        env->SetIntArrayRegion(resultArray, 0, result.size(), (jint*)result.data());
    }
    
    return resultArray;
}