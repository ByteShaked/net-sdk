#include "crypto_util.h"
// #include <stdexcept>


const string Cryptos::SECRET_KEY = "06dc87af5f37a004da50ceeb32a1b9c7";
const string Cryptos::SECRET_CONF_KEY = "10C173A98BAD2FF723BE0E81A9D85965";
const int Cryptos::rev[16] = {0x0b, 0x04, 0x0f, 0x06, 0x01, 0x0a, 0x03, 0x09, 
                          0x0d, 0x07, 0x05, 0x00, 0x0e, 0x08, 0x0c, 0x02};
const int Cryptos::sk[16] = {0xd7, 0x6a, 0xa4, 0x78, 0xf5, 0x7c, 0x42, 0xab, 
                         0xa4, 0x52, 0xf6, 0x76, 0x3b, 0x4d, 0x61, 0xce};

 int Cryptos::Byte2Int(uint8_t b) {
    return static_cast<int>(b);
}

 int Cryptos::ByteArrayToInt(const uint8_t bytes[4]) {
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

 string Cryptos::crypto_quick_dec_ByteArr_Tx(const vm_crypto_quick_key_t& key, const vector<uint8_t>& src) {
    vector<int> decrypted = crypto_quick_dec(key, src, src.size());
    string result;
    for (int val : decrypted) {
        result.push_back(static_cast<char>(val));
    }
    return result;
}

 vector<uint8_t> Cryptos::crypto_quick_dec_ByteArr_Tx(const vm_crypto_quick_key_t& key, const vector<uint8_t>& src, int len) {
    vector<int> decrypted = crypto_quick_dec(key, src, len);
    vector<uint8_t> result;
    for (int val : decrypted) {
        result.push_back(static_cast<uint8_t>(val));
    }
    return result;
}

 vector<int> Cryptos::crypto_quick_dec(const vm_crypto_quick_key_t& key, const vector<uint8_t>& src, int len) {
    int lenf = len;
    vector<int> dst(len + 8, 0);
    int r[8], rk[8], iv[8];
    memcpy(rk, key.getRk(), sizeof(rk));
    memcpy(iv, key.getIv(), sizeof(iv));

    int src_f = 0, dst_f = 0;
    while (lenf > 0) {
        for (int n = 0; n < 8; n++) {
            int src_val = (src_f + n < src.size()) ? src[src_f + n] : 0;
            r[n] = src_val ^ rk[n] ^ sk[n];
            int transformed = (rev[r[n] >> 4] + (rev[r[n] & 0xF] << 4)) ^ iv[n];
            dst[dst_f + 7 - n] = transformed;
            iv[n] = src_val;
        }
        lenf -= 8;
        src_f += 8;
        dst_f += 8;
    }
    return vector<int>(dst.begin(), dst.begin() + len);
}

 vector<uint8_t> Cryptos::vm_crypto_quick_enc_ByteArr(const vm_crypto_quick_key_t& key, const vector<uint8_t>& src) {
    vector<int> encrypted = vm_crypto_quick_enc(key, src);
    vector<uint8_t> result;
    for (int val : encrypted) {
        result.push_back(static_cast<uint8_t>(val));
    }
    return result;
}

 vector<int> Cryptos::vm_crypto_quick_enc(const vm_crypto_quick_key_t& key, const vector<uint8_t>& src) {
    int len = src.size();
    vector<int> dst(len + 8, 0);
    int rk[8], iv[8];
    memcpy(rk, key.getRk(), sizeof(rk));
    memcpy(iv, key.getIv(), sizeof(iv));

    int src_f = 0, dst_f = 0;
    while (len > 0) {
        for (int n = 0; n < 8; n++) {
            int idx = src_f + 7 - n;
            iv[n] ^= (idx < src.size()) ? src[idx] : 0;
            iv[n] = (rev[iv[n] >> 4] + (rev[iv[n] & 0xF] << 4)) ^ rk[n] ^ sk[n];
            dst[dst_f + n] = iv[n];
        }
        len -= 8;
        src_f += 8;
        dst_f += 8;
    }
    return vector<int>(dst.begin(), dst.begin() + dst_f);
}

Cryptos::vm_crypto_quick_key_t Cryptos::vm_crypto_quick_init_key_Str(const string& userkeyTxt) {
    vector<int> userkey;
    for (char c : userkeyTxt) {
        userkey.push_back(static_cast<int>(c));
    }
    return vm_crypto_quick_init_key(userkey);
}

 vector<uint8_t> Cryptos::toQESEncode(const string& key, const vector<uint8_t>& srcTx) {
    vm_crypto_quick_key_t keyt = vm_crypto_quick_init_key_Str(key);
    return vm_crypto_quick_enc_ByteArr(keyt, srcTx);
}

vector<uint8_t> Cryptos::toQESDecode(const string& key, const vector<uint8_t>& byteTx) {
    vm_crypto_quick_key_t keyt = vm_crypto_quick_init_key_Str(key);
    return crypto_quick_dec_ByteArr_Tx(keyt, byteTx, byteTx.size());
}

Cryptos::vm_crypto_quick_key_t Cryptos::vm_crypto_quick_init_key(const vector<int>& userkey) {
    const int __smask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
    int ukey[8] = {0};
    vm_crypto_quick_key_t key;

    for (size_t i = 0; i < userkey.size(); i++) {
        ukey[i & 0x7] ^= userkey[i];
    }

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            int pos = (i + j) & 0x07;
            key.rk[i] |= ukey[pos] & __smask[(i + j) & 0x07];
        }
    }
    return key;
}

