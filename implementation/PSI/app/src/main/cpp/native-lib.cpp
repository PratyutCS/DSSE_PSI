/**
 * native-lib.cpp — Final JNI bridge integrating logic_source_code
 * Handles Binary safety (Base64), Memory safety (LocalRefs), and RocksDB Persistence.
 */
#include <jni.h>
#include <string>
#include <vector>
#include <tuple>
#include <algorithm>
#include <fstream>
#include <bitset>
#include <android/log.h>

#include "Lambda_Optimized/src/Lambda.h"

#include <cryptopp/osrng.h>
#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/base64.h>

#define TAG "PSI_NATIVE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// --------------- Binary Safe Helpers ---------------

static std::string toBase64(const std::string &input) {
    std::string encoded;
    CryptoPP::StringSource ss(input, true,
        new CryptoPP::Base64Encoder(new CryptoPP::StringSink(encoded), false));
    return encoded;
}

static std::string fromBase64(const std::string &input) {
    std::string decoded;
    try {
        CryptoPP::StringSource ss(input, true,
            new CryptoPP::Base64Decoder(new CryptoPP::StringSink(decoded)));
    } catch (...) { LOGE("Base64 decoding failed"); }
    return decoded;
}

// --------------- File Encryption (AES-CBC) ---------------

static void encryptFileCBC(const CryptoPP::SecByteBlock &key, const std::string &inPath, const std::string &outPath) {
    using namespace CryptoPP;
    std::ifstream ifs(inPath, std::ios::binary);
    if (!ifs) return;
    std::string plain((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    SecByteBlock iv(AES::BLOCKSIZE);
    AutoSeededRandomPool prng;
    prng.GenerateBlock(iv, iv.size());
    std::string cipher;
    CBC_Mode<AES>::Encryption enc;
    enc.SetKeyWithIV(key, key.size(), iv);
    StringSource(plain, true, new StreamTransformationFilter(enc, new StringSink(cipher)));
    std::ofstream ofs(outPath, std::ios::binary);
    ofs.write((const char*)iv.data(), iv.size());
    ofs.write(cipher.data(), cipher.size());
}

static void decryptFileCBC(const CryptoPP::SecByteBlock &key, const std::string &inPath, const std::string &outPath) {
    using namespace CryptoPP;
    std::ifstream ifs(inPath, std::ios::binary);
    if (!ifs) return;
    std::string raw((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (raw.size() < (size_t)AES::BLOCKSIZE) return;
    unsigned char iv[AES::BLOCKSIZE];
    memcpy(iv, raw.data(), AES::BLOCKSIZE);
    std::string cipherOnly = raw.substr(AES::BLOCKSIZE);
    std::string plain;
    CBC_Mode<AES>::Decryption dec;
    dec.SetKeyWithIV(key, key.size(), iv);
    try { StringSource(cipherOnly, true, new StreamTransformationFilter(dec, new StringSink(plain))); } catch(...) { return; }
    std::ofstream ofs(outPath, std::ios::binary);
    ofs.write(plain.data(), plain.size());
}

// --------------- DSSE LifeCycle ---------------

static DSSE* g_dsse = nullptr;

static void initDSSE(const std::string &basePath, const unsigned char* keyData, size_t keyLen) {
    if (g_dsse) { delete g_dsse; g_dsse = nullptr; }
    g_dsse = new DSSE();
    if (keyData && keyLen >= 16) {
        CryptoPP::SecByteBlock sk(keyData, 16);
        g_dsse->Set_Client_sk(sk);
        g_dsse->Data.client_sk = sk;
    }
    rocksdb::Options opts;
    opts.create_if_missing = true;
    std::string sigmaPath = basePath + "/Sigma_map1";
    rocksdb::DB* db1 = nullptr;
    rocksdb::Status s = rocksdb::DB::Open(opts, sigmaPath, &db1);
    if (!s.ok()) LOGE("RocksDB Error: %s", s.ToString().c_str());
    g_dsse->Data.map1 = db1;
    g_dsse->Data.map2 = nullptr;
}

extern "C" {

JNIEXPORT void JNICALL
Java_com_example_psi_MainActivity_initializeNative(JNIEnv *env, jobject, jstring storagePath) {}

JNIEXPORT jobjectArray JNICALL
Java_com_example_psi_UpdateActivity_generateUpdateTokens(
        JNIEnv *env, jobject, jstring jStoragePath, jbyteArray jKey, jint kwSpaceSize,
        jobjectArray jFilePaths, jintArray jKeywords, jint startingIdIndex) {

    const char *sPath = env->GetStringUTFChars(jStoragePath, nullptr);
    jbyte* kData = env->GetByteArrayElements(jKey, nullptr);
    initDSSE(sPath, (const unsigned char*)kData, (size_t)env->GetArrayLength(jKey));

    jsize numFiles = env->GetArrayLength(jFilePaths);
    jint *kws = env->GetIntArrayElements(jKeywords, nullptr);

    std::vector<int> KS;
    for (int i = 0; i < (int)kwSpaceSize; i++) KS.push_back(i);

    std::vector<std::string> allTokens;
    std::vector<std::string> encPaths;

    for (int i = 0; i < numFiles; i++) {
        jstring js = (jstring)env->GetObjectArrayElement(jFilePaths, i);
        const char *fPath = env->GetStringUTFChars(js, nullptr);

        int currentId = (int)startingIdIndex + i;
        std::string ind = "ID" + std::to_string(currentId);
        std::string outPath = std::string(sPath) + "/" + ind;
        encryptFileCBC(g_dsse->Get_Client_sk(), fPath, outPath);
        encPaths.push_back(outPath);

        std::vector<std::tuple<std::string, std::string>> U_list;
        Update_client(*g_dsse, true, ind, currentId, (int)kws[i], KS, U_list);

        for (auto &tok : U_list) {
            allTokens.push_back(toBase64(std::get<0>(tok)));
            allTokens.push_back(toBase64(std::get<1>(tok)));
        }
        env->ReleaseStringUTFChars(js, fPath);
        env->DeleteLocalRef(js);
    }

    int total = (int)allTokens.size() + 1 + (int)encPaths.size();
    jclass strCls = env->FindClass("java/lang/String");
    jobjectArray ret = env->NewObjectArray(total, strCls, nullptr);

    int idx = 0;
    for (auto &t : allTokens) {
        jstring js = env->NewStringUTF(t.c_str());
        env->SetObjectArrayElement(ret, idx++, js);
        env->DeleteLocalRef(js);
    }
    jstring sep = env->NewStringUTF("---FILES---");
    env->SetObjectArrayElement(ret, idx++, sep);
    env->DeleteLocalRef(sep);
    for (auto &p : encPaths) {
        jstring js = env->NewStringUTF(p.c_str());
        env->SetObjectArrayElement(ret, idx++, js);
        env->DeleteLocalRef(js);
    }

    env->ReleaseByteArrayElements(jKey, kData, JNI_ABORT);
    env->ReleaseIntArrayElements(jKeywords, kws, JNI_ABORT);
    env->ReleaseStringUTFChars(jStoragePath, sPath);
    return ret;
}

JNIEXPORT jobjectArray JNICALL
Java_com_example_psi_UpdateActivity_generateDeleteTokens(
        JNIEnv *env, jobject, jstring jStoragePath, jbyteArray jKey, jint kwSpaceSize,
        jobjectArray jIdentifiers, jintArray jKeywords) {

    const char *sPath = env->GetStringUTFChars(jStoragePath, nullptr);
    jbyte* kData = env->GetByteArrayElements(jKey, nullptr);
    initDSSE(sPath, (const unsigned char*)kData, (size_t)env->GetArrayLength(jKey));

    jsize n = env->GetArrayLength(jIdentifiers);
    jint *kws = env->GetIntArrayElements(jKeywords, nullptr);
    std::vector<int> KS;
    for (int i = 0; i < (int)kwSpaceSize; i++) KS.push_back(i);
    std::vector<std::string> allTokens;

    for (int i = 0; i < n; i++) {
        jstring js = (jstring)env->GetObjectArrayElement(jIdentifiers, i);
        const char *idRaw = env->GetStringUTFChars(js, nullptr);
        std::string ind(idRaw);
        env->ReleaseStringUTFChars(js, idRaw);
        int indIndex = 0;
        try { if(ind.size()>2) indIndex = std::stoi(ind.substr(2)); } catch (...) {}
        std::vector<std::tuple<std::string, std::string>> U_list;
        Update_client(*g_dsse, false, ind, indIndex, (int)kws[i], KS, U_list);
        for (auto &tok : U_list) {
            allTokens.push_back(toBase64(std::get<0>(tok)));
            allTokens.push_back(toBase64(std::get<1>(tok)));
        }
        env->DeleteLocalRef(js);
    }

    jobjectArray ret = env->NewObjectArray((int)allTokens.size(), env->FindClass("java/lang/String"), nullptr);
    for (int i = 0; i < (int)allTokens.size(); i++) {
        jstring js = env->NewStringUTF(allTokens[i].c_str());
        env->SetObjectArrayElement(ret, i, js);
        env->DeleteLocalRef(js);
    }
    env->ReleaseByteArrayElements(jKey, kData, JNI_ABORT);
    env->ReleaseIntArrayElements(jKeywords, kws, JNI_ABORT);
    env->ReleaseStringUTFChars(jStoragePath, sPath);
    return ret;
}

JNIEXPORT jobjectArray JNICALL
Java_com_example_psi_SearchActivity_getSearchTokens(
        JNIEnv *env, jobject, jstring jStoragePath, jbyteArray jKey, jint a, jint b, jint kwSpaceSize) {

    const char *sPath = env->GetStringUTFChars(jStoragePath, nullptr);
    jbyte* kData = env->GetByteArrayElements(jKey, nullptr);
    initDSSE(sPath, (const unsigned char*)kData, (size_t)env->GetArrayLength(jKey));

    std::vector<int> KS;
    for (int i = 0; i < (int)kwSpaceSize; i++) KS.push_back(i);

    std::tuple<std::string,std::string,int> stk1_l, stk1_e, stk2_l, stk2_e;
    Search_client(*g_dsse, std::make_tuple((int)a, (int)b), KS, stk1_l, stk1_e, stk2_l, stk2_e);

    jobjectArray ret = env->NewObjectArray(12, env->FindClass("java/lang/String"), nullptr);
    auto fill = [&](int off, const std::tuple<std::string,std::string,int> &t) {
        jstring js0 = env->NewStringUTF(toBase64(std::get<0>(t)).c_str());
        jstring js1 = env->NewStringUTF(toBase64(std::get<1>(t)).c_str());
        jstring js2 = env->NewStringUTF(std::to_string(std::get<2>(t)).c_str());
        env->SetObjectArrayElement(ret, off, js0);
        env->SetObjectArrayElement(ret, off+1, js1);
        env->SetObjectArrayElement(ret, off+2, js2);
        env->DeleteLocalRef(js0); env->DeleteLocalRef(js1); env->DeleteLocalRef(js2);
    };
    fill(0, stk1_l); fill(3, stk1_e); fill(6, stk2_l); fill(9, stk2_e);

    env->ReleaseByteArrayElements(jKey, kData, JNI_ABORT);
    env->ReleaseStringUTFChars(jStoragePath, sPath);
    return ret;
}

JNIEXPORT jintArray JNICALL
Java_com_example_psi_SearchActivity_performPostProcessing(
        JNIEnv *env, jobject, jint numIDs, jint kwSpaceSize,
        jobjectArray jR1l, jobjectArray jR1e, jobjectArray jR2l, jobjectArray jR2e,
        jint a, jint b) {

    auto toVec = [&](jobjectArray arr) -> std::vector<std::string> {
        std::vector<std::string> v; if (!arr) return v;
        jsize len = env->GetArrayLength(arr);
        for (int i = 0; i < len; i++) {
            jstring js = (jstring)env->GetObjectArrayElement(arr, i);
            const char *s = env->GetStringUTFChars(js, nullptr);
            v.push_back(fromBase64(s ? s : ""));
            env->ReleaseStringUTFChars(js, s); env->DeleteLocalRef(js);
        }
        return v;
    };
    auto res1_l = toVec(jR1l); auto res2_l = toVec(jR2l); auto res2_e = toVec(jR2e);
    int N = (int)numIDs; int m = (int)kwSpaceSize;
    auto depad = [](std::string s) -> std::string {
        size_t p = s.find_last_not_of('_');
        return (p != std::string::npos) ? s.substr(0, p+1) : "";
    };
    std::vector<bool> a_lbm(N, false), b_lbm(N, false), b_ebm(N, false);
    for (auto &id : res1_l) { std::string d = depad(id); try { int idx = std::stoi(d); if (idx >= 0 && idx < N) a_lbm[idx] = true; } catch (...) {} }
    if ((int)a > m/2) { for (int i = 0; i < N; i++) a_lbm[i] = !a_lbm[i]; }
    for (auto &id : res2_l) { std::string d = depad(id); try { int idx = std::stoi(d); if (idx >= 0 && idx < N) b_lbm[idx] = true; } catch (...) {} }
    if ((int)b > m/2) { for (int i = 0; i < N; i++) b_lbm[i] = !b_lbm[i]; }
    for (auto &id : res2_e) { std::string d = depad(id); try { int idx = std::stoi(d); if (idx >= 0 && idx < N) b_ebm[idx] = true; } catch (...) {} }
    std::vector<int> matched;
    for (int i = 0; i < N; i++) { if (!a_lbm[i] && (b_lbm[i] || b_ebm[i])) matched.push_back(i); }
    jintArray result = env->NewIntArray((int)matched.size());
    if (!matched.empty()) env->SetIntArrayRegion(result, 0, (int)matched.size(), matched.data());
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_example_psi_SearchActivity_decryptResultFile(
        JNIEnv *env, jobject, jstring jStoragePath, jbyteArray jKey, jstring jEncPath, jstring jDecPath) {
    const char *sPath = env->GetStringUTFChars(jStoragePath, nullptr);
    const char *encP = env->GetStringUTFChars(jEncPath, nullptr);
    const char *decP = env->GetStringUTFChars(jDecPath, nullptr);
    jbyte* kData = env->GetByteArrayElements(jKey, nullptr);
    initDSSE(sPath, (const unsigned char*)kData, (size_t)env->GetArrayLength(jKey));
    jboolean ok = JNI_FALSE;
    if (g_dsse) { try { decryptFileCBC(g_dsse->Get_Client_sk(), encP, decP); ok = JNI_TRUE; } catch(...) {} }
    env->ReleaseByteArrayElements(jKey, kData, JNI_ABORT);
    env->ReleaseStringUTFChars(jStoragePath, sPath); env->ReleaseStringUTFChars(jEncPath, encP); env->ReleaseStringUTFChars(jDecPath, decP);
    return ok;
}

JNIEXPORT void JNICALL
Java_com_example_psi_DeleteSpaceActivity_clearNativeDB(JNIEnv *env, jobject, jstring jStoragePath) {
    if (g_dsse) { delete g_dsse; g_dsse = nullptr; }
}

} // extern "C"
