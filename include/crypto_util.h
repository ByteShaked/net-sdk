// Cryptos.h
#ifndef CRYPTOS_H
#define CRYPTOS_H


#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <stdexcept>

using namespace std;

class Cryptos {
public:
    static const string SECRET_KEY;
    static const string SECRET_CONF_KEY;
    static vector<uint8_t> toQESEncode(const string& key, const vector<uint8_t>& srcTx);
    vector<uint8_t> toQESDecode(const string& key, const vector<uint8_t>& byteTx);

private:
    static const int rev[16];
    static const int sk[16];

    struct vm_crypto_quick_key_t {
        int iv[8];
        int rk[8];

        vm_crypto_quick_key_t() {
            memset(iv, 0, sizeof(iv));
            memset(rk, 0, sizeof(rk));
        }

        const int* getIv() const { return iv; }
        const int* getRk() const { return rk; }
        void setIv(const int* src) { memcpy(iv, src, sizeof(iv)); }
        void setRk(const int* src) { memcpy(rk, src, sizeof(rk)); }
    };

    static int Byte2Int(uint8_t b);
    static int ByteArrayToInt(const uint8_t bytes[4]);

    static string crypto_quick_dec_ByteArr_Tx(const vm_crypto_quick_key_t& key, const vector<uint8_t>& src);

    static vector<uint8_t> crypto_quick_dec_ByteArr_Tx(const vm_crypto_quick_key_t& key, const vector<uint8_t>& src, int len);
    static vector<int> crypto_quick_dec(const vm_crypto_quick_key_t& key, const vector<uint8_t>& src, int len);

    static vector<uint8_t> vm_crypto_quick_enc_ByteArr(const vm_crypto_quick_key_t& key, const vector<uint8_t>& src);

    static vector<int> vm_crypto_quick_enc(const vm_crypto_quick_key_t& key, const vector<uint8_t>& src);

    static vm_crypto_quick_key_t vm_crypto_quick_init_key_Str(const string& userkeyTxt);


    static vm_crypto_quick_key_t vm_crypto_quick_init_key(const vector<int>& userkey);
};
#endif