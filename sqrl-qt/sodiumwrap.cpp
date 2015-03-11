#include <sodium.h>
#include "sodiumwrap.h"
#include <QDebug>

static unsigned char* getUnsignedCharFromString(QString str) {
  int len = str.length();
  unsigned char* result = new unsigned char[len];
  for (int i = 0; i < len; ++i) {
    result[i] = (unsigned char)str.at(i).toAscii();
  }
  return result;
}

unsigned char* SodiumWrap::getKeyFromQString(QString input) {
  return getUnsignedCharFromString(input);
}

unsigned char* SodiumWrap::hmacSha256(unsigned char* key, QString message) {
  unsigned char* out = new unsigned char[crypto_auth_hmacsha256_BYTES];
  unsigned char* in = getUnsignedCharFromString(message);

  crypto_auth_hmacsha256(out, in, message.length(), key);

  if (crypto_auth_hmacsha256_verify(out, in, message.length(), key) != 0) {
    qDebug() << "Error! HMAC failed!";
    return NULL;
  }

  return out;
}

unsigned char* SodiumWrap::signDetached(QString message,
                                        unsigned char* privateKey,
                                        unsigned char* publicKey) {
  unsigned char* actualMessage = getUnsignedCharFromString(message);
  unsigned char* out = new unsigned char[SodiumWrap::SIG_LEN];

  crypto_sign_detached(out, NULL, actualMessage, message.length(), privateKey);

  if (crypto_sign_verify_detached(out, actualMessage, message.length(),
                                  publicKey) != 0) {
    qDebug() << "Signing failed!";
    return NULL;
  }

  return out;
}

unsigned char* SodiumWrap::generatePrivateKey(QByteArray seed) {
  // Prepare the seed
  unsigned char actualSeed[SodiumWrap::SEED_LEN];
  memcpy(actualSeed, seed, SodiumWrap::SEED_LEN);

  // Prepare public and private keys
  unsigned char* privateKey = new unsigned char[SodiumWrap::SK_LEN];
  unsigned char publicKey[SodiumWrap::PK_LEN];

  // Generate keys from seed
  crypto_sign_seed_keypair(publicKey, privateKey, actualSeed);

  return privateKey;
}

unsigned char* SodiumWrap::ed25519PrivateKeyToPublicKey(unsigned char* privateKey) {
  unsigned char* publicKey = new unsigned char[SodiumWrap::PK_LEN];

  crypto_sign_ed25519_sk_to_pk(publicKey, privateKey);

  return publicKey;
}