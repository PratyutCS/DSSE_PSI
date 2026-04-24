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
#include <future>
#include <mutex>
#include <sys/stat.h>
#include <android/log.h>

#include "Lambda_Optimized/src/Lambda.h"

#include <cryptopp/osrng.h>
#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/base64.h>
#include <cryptopp/files.h>

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
static std::mutex g_update_mutex;

static void encryptFileCBC(const CryptoPP::SecByteBlock &key, const std::string &inPath, const std::string &outPath) {
    try {
        CryptoPP::AutoSeededRandomPool prng;
        CryptoPP::byte iv[CryptoPP::AES::BLOCKSIZE];
        prng.GenerateBlock(iv, sizeof(iv));

        CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption enc;
        enc.SetKeyWithIV(key, key.size(), iv);

        std::ofstream ofs(outPath, std::ios::binary);
        if (!ofs) {
            LOGE("Encryption Error: Could not open output file %s", outPath.c_str());
            return;
        }
        ofs.write((const char*)iv, sizeof(iv));

        CryptoPP::FileSource fs(inPath.c_str(), true,
            new CryptoPP::StreamTransformationFilter(enc,
                new CryptoPP::FileSink(ofs)
            )
        );
    } catch (const CryptoPP::Exception &e) {
        LOGE("Encryption CryptoPP Error: %s", e.what());
    } catch (const std::exception &e) {
        LOGE("Encryption Std Error: %s", e.what());
    }
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
static std::string g_lastPath = "";

static void initDSSE(const std::string &basePath, const unsigned char* keyData, size_t keyLen) {
    if (g_dsse && g_lastPath == basePath) {
        // Already initialized for this space, just return.
        return;
    }

    if (g_dsse) {
        delete g_dsse;
        g_dsse = nullptr;
    }

    g_dsse = new DSSE();
    g_lastPath = basePath;

    if (keyData && keyLen >= 16) {
        CryptoPP::SecByteBlock sk(keyData, 16);
        g_dsse->Set_Client_sk(sk);
        g_dsse->Data.client_sk = sk;
    }

    rocksdb::Options opts;
    opts.create_if_missing = true;
    
    // Performance Tweak: Increase parallelism for mobile
    opts.IncreaseParallelism();
    opts.OptimizeLevelStyleCompaction();
    
    std::string sigmaPath = basePath + "/Sigma_map1";
    rocksdb::DB* db1 = nullptr;
    rocksdb::Status s = rocksdb::DB::Open(opts, sigmaPath, &db1);
    if (!s.ok()) {
        LOGE("RocksDB Error: %s", s.ToString().c_str());
    }
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

    const char *sPathChars = env->GetStringUTFChars(jStoragePath, nullptr);
    std::string basePath(sPathChars);
    env->ReleaseStringUTFChars(jStoragePath, sPathChars);

    jbyte* kData = env->GetByteArrayElements(jKey, nullptr);
    initDSSE(basePath, (const unsigned char*)kData, (size_t)env->GetArrayLength(jKey));
    env->ReleaseByteArrayElements(jKey, kData, JNI_ABORT);

    if (!g_dsse) return nullptr;

    jsize numFiles = env->GetArrayLength(jFilePaths);
    jint *kws = env->GetIntArrayElements(jKeywords, nullptr);

    std::vector<std::string> filePaths;
    for (int i = 0; i < numFiles; i++) {
        jstring js = (jstring)env->GetObjectArrayElement(jFilePaths, i);
        const char *f = env->GetStringUTFChars(js, nullptr);
        filePaths.push_back(std::string(f));
        env->ReleaseStringUTFChars(js, f);
        env->DeleteLocalRef(js);
    }

    std::vector<int> KS;
    for (int i = 0; i < (int)kwSpaceSize; i++) KS.push_back(i);

    std::string encDir = basePath + "/encrypted";
    mkdir(encDir.c_str(), 0777);

    // Parallelize Encryption and Token Generation
    std::vector<std::future<std::vector<std::tuple<std::string, std::string>>>> futures;

    for (int i = 0; i < (int)numFiles; i++) {
        int kw = kws[i];
        futures.push_back(std::async(std::launch::async, [=, &filePaths, &KS]() {
            int currentId = (int)startingIdIndex + i;
            std::string ind = "ID" + std::to_string(currentId);
            std::string fPath = filePaths[i];
            
            // Extract extension
            std::string ext = "";
            size_t dotPos = fPath.find_last_of(".");
            if (dotPos != std::string::npos) {
                ext = fPath.substr(dotPos);
            }
            std::string outPath = encDir + "/" + ind + ext;

            // 1. Parallel Encryption - Renaming to ID{N}.ext
            encryptFileCBC(g_dsse->Data.client_sk, fPath, outPath);

            // 2. Parallel Token Generation
            std::vector<std::tuple<std::string, std::string>> U_list;
            {
                std::lock_guard<std::mutex> lock(g_update_mutex);
                Update_client(*g_dsse, true, ind, currentId, kw, KS, U_list);
            }
            return U_list;
        }));
    }

    std::vector<std::tuple<std::string, std::string>> total_list;
    for (auto &f : futures) {
        auto res = f.get();
        total_list.insert(total_list.end(), res.begin(), res.end());
    }

    env->ReleaseIntArrayElements(jKeywords, kws, JNI_ABORT);

    // Prepare java result
    jclass pairClass = env->FindClass("com/example/psi/network/IndexPair");
    if (!pairClass) {
        LOGE("IndexPair class not found!");
        return nullptr;
    }
    jmethodID pairConstructor = env->GetMethodID(pairClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
    
    jobjectArray result = env->NewObjectArray((jsize)total_list.size(), pairClass, nullptr);
    for (size_t i = 0; i < total_list.size(); i++) {
        std::string key64 = toBase64(std::get<0>(total_list[i]));
        std::string val64 = toBase64(std::get<1>(total_list[i]));
        jobject pair = env->NewObject(pairClass, pairConstructor, env->NewStringUTF(key64.c_str()), env->NewStringUTF(val64.c_str()));
        env->SetObjectArrayElement(result, (jsize)i, pair);
        env->DeleteLocalRef(pair);
    }

    return result;
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

    LOGI("Search_client called with range_query: [%d, %d]", (int)a, (int)b);

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

JNIEXPORT jobjectArray JNICALL
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
    auto res1_l = toVec(jR1l); auto res1_e = toVec(jR1e);
    auto res2_l = toVec(jR2l); auto res2_e = toVec(jR2e);
    int N = (int)numIDs; int m = (int)kwSpaceSize;
    auto depad = [](std::string s) -> std::string {
        size_t p = s.find_last_not_of('_');
        return (p != std::string::npos) ? s.substr(0, p+1) : "";
    };

    std::vector<bool> res1_raw(N, false), res2_raw(N, false), b_ebm(N, false);

    auto processRes = [&](const std::vector<std::string>& res, std::vector<bool>& bitmap) {
        for (auto &id : res) {
            std::string d = depad(id);
            try {
                int val = std::stoi(d);
                int suffix = val % 10;
                int idx = val / 10;
                if (suffix == 1 && idx >= 0 && idx < N) bitmap[idx] = true;
            } catch (...) {}
        }
    };

    processRes(res1_l, res1_raw);
    processRes(res2_l, res2_raw);
    processRes(res2_e, b_ebm);

    // Cross-check all result sets for any '0' suffix (deletions)
    auto handleDeletions = [&](const std::vector<std::string>& res) {
        for (auto &id : res) {
            std::string d = depad(id);
            try {
                int val = std::stoi(d);
                if (val % 10 == 0) {
                    int idx = val / 10;
                    if (idx >= 0 && idx < N) {
                        res1_raw[idx] = false;
                        res2_raw[idx] = false;
                        b_ebm[idx] = false;
                    }
                }
            } catch (...) {}
        }
    };
    handleDeletions(res1_l); handleDeletions(res1_e);
    handleDeletions(res2_l); handleDeletions(res2_e);

    std::vector<bool> a_ge_bm(N, false);
    if (a <= m / 2) {
        // When a <= m/2, res1_l gives us less-than `a`. So we invert to get >= `a`.
        for (int i = 0; i < N; i++) a_ge_bm[i] = !res1_raw[i];
    } else {
        // When a > m/2, res1_l inherently gives us >= `a`. We use it directly.
        for (int i = 0; i < N; i++) a_ge_bm[i] = res1_raw[i];
    }
    
    std::string b1_type = "ge"; // Greater than or Equal
    std::string b1_raw_str = "";
    for (int i = 0; i < N; i++) b1_raw_str += a_ge_bm[i] ? "1" : "0";
    LOGI("Search 1 (%s) bitmap: %s", b1_type.c_str(), b1_raw_str.c_str());

    std::vector<bool> b_lbm(N, false);
    if (b <= m / 2) {
        // When b <= m/2, res2_l gives us less-than `b`. Used directly.
        for (int i = 0; i < N; i++) b_lbm[i] = res2_raw[i];
    } else {
        // When b > m/2, res2_l gives us >= `b`. Invert to get less-than `b`.
        for (int i = 0; i < N; i++) b_lbm[i] = !res2_raw[i];
    }
    
    std::string b2_type = "l";
    std::string b2_raw_str = "";
    for (int i = 0; i < N; i++) b2_raw_str += b_lbm[i] ? "1" : "0";
    LOGI("Search 2 (%s) bitmap: %s", b2_type.c_str(), b2_raw_str.c_str());
    
    std::string b3_type = "e";
    std::string b3_raw_str = "";
    for (int i = 0; i < N; i++) b3_raw_str += b_ebm[i] ? "1" : "0";
    LOGI("Search 2 (%s) bitmap: %s", b3_type.c_str(), b3_raw_str.c_str());

    // Compute Less Than or Equal (LE) for Search 2
    std::vector<bool> b_le_bm(N, false);
    std::string b_le_str = "";
    for (int i = 0; i < N; i++) {
        b_le_bm[i] = b_lbm[i] || b_ebm[i];
        b_le_str += b_le_bm[i] ? "1" : "0";
    }

    std::vector<int> matched;
    std::string resultant_str = "";
    for (int i = 0; i < N; i++) { 
        // Final logical intersection: Greater-than-or-Equal (a) AND Less-than-or-Equal (b)
        bool is_match = a_ge_bm[i] && b_le_bm[i];
        resultant_str += is_match ? "1" : "0";
        if (is_match) matched.push_back(i); 
    }
    
    LOGI("Resultant bitmap:     %s", resultant_str.c_str());

    std::vector<std::string> resultsVec;
    resultsVec.push_back(b1_type + ") bitmap: " + b1_raw_str);
    resultsVec.push_back(b2_type + ") bitmap: " + b2_raw_str);
    resultsVec.push_back(b3_type + ") bitmap: " + b3_raw_str);
    resultsVec.push_back(resultant_str);

    for (int id : matched) {
        resultsVec.push_back(std::to_string(id));
    }

    jobjectArray result = env->NewObjectArray((int)resultsVec.size(), env->FindClass("java/lang/String"), nullptr);
    for (size_t i = 0; i < resultsVec.size(); i++) {
        jstring js = env->NewStringUTF(resultsVec[i].c_str());
        env->SetObjectArrayElement(result, i, js);
        env->DeleteLocalRef(js);
    }
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
