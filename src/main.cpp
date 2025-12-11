#include <sodium.h>
#include <iostream>
#include <string>
#include <array>
#include <fstream>
#include "types.h"
#include "crypto_utils.h"


using namespace std;
array<uint8_t, crypto_box_SECRETKEYBYTES> bytesPrivateKey;
array<uint8_t, crypto_box_PUBLICKEYBYTES> bytesPublicKey;
string hexPrivateKey;
string hexPublicKey;
array<uint8_t, crypto_sign_BYTES> signature;
string userInput;

int main()
{
    cout << time(0);
    cout << "Enter your private key or type new to generate one: ";
    cin >> userInput;
    if (userInput == "new")
    {
        crypto_box_keypair(bytesPublicKey.data(), bytesPrivateKey.data());
        bytesToHex(hexPublicKey, bytesPublicKey, crypto_box_PUBLICKEYBYTES);
        bytesToHex(hexPrivateKey, bytesPrivateKey, crypto_box_SECRETKEYBYTES);
    }
    else
    {
        hexToBytes(bytesPrivateKey, userInput);
        crypto_scalarmult_base(bytesPublicKey.data(), bytesPrivateKey.data());
        bytesToHex(hexPublicKey, bytesPublicKey, crypto_box_PUBLICKEYBYTES);
        bytesToHex(hexPrivateKey, bytesPrivateKey, crypto_box_SECRETKEYBYTES);
    }
    cout << "Public Key: " << hexPublicKey << endl << "Private Key: " << hexPrivateKey << endl;
}
