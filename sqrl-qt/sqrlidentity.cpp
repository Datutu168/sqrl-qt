#include "sqrlidentity.h"
#include <QDebug>
#include <QString>
#include <QFile>
#include <QDir>
#include <QByteArray>
#include <QStringList>
#include <sodium.h>

SqrlIdentity::SqrlIdentity() {
}

static unsigned char* getUnsignedCharFromString(QString str, int len) {
  unsigned char* result = new unsigned char[crypto_sign_SEEDBYTES];
  for (int i = 0; i < len; ++i) {
    result[i] = (unsigned char)str.at(i).toAscii();
  }
  return result;
}

/*
 * Generate a new SQRL identity.
 * Need to add **LOTS** of entropy here.
 */
bool SqrlIdentity::createIdentity() {
  qDebug() << "LOTS of entropy goes here";
  qDebug() << "Security warning: don't use this key for anything but testing!";

  qDebug() << "Currently the key is HARD-CODED!! Very bad!!";
  QString seed = "0123456789ABCDEF0123456789ABCDEF";
  this->key = getUnsignedCharFromString(seed, seed.length());

  QString folderName = QDir::homePath() + "/.sqrl";
  if (!QDir(folderName).exists())
    QDir().mkdir(folderName);

  QString fileName = "ident.txt";
  QFile file(folderName + "/" + fileName);

  if (file.open(QIODevice::WriteOnly)) {
    QTextStream out(&file);
    for (unsigned int i = 0; i < crypto_sign_SEEDBYTES; ++i) {
      out << (char)this->key[i];
    }
    file.close();
  }
  else {
    qDebug() << "Error: couldn't open file for writing.";
  }

  return true;
}

/*
 * Load a SQRL identity from file.
 * In case of failure, return false, otherwise, return true.
 */
bool SqrlIdentity::loadIdentity() {
  QString filename = QDir::homePath() + "/.sqrl/ident.txt";
  QFile file(filename);

  if (file.size() == crypto_sign_SEEDBYTES) {
    if (file.open(QIODevice::ReadOnly)) {
      QTextStream in(&file);
      QString seed = in.readAll();
      this->key = getUnsignedCharFromString(seed, seed.length());
      file.close();

      return true;
    }
  }

  return false;
}

unsigned char* SqrlIdentity::getKey() {
  return this->key;
}

static QString getStringFromUnsignedChar(unsigned char *str) {
  QString result = "";

  for (unsigned int i = 0; i < crypto_sign_SEEDBYTES; i++) {
    QChar c = str[i];
    result.append(c);
  }

  return result;
}

QString SqrlIdentity::getHexKey() {
  return getStringFromUnsignedChar(this->key);
}

QByteArray SqrlIdentity::makeDomainPrivateKey(QString domain) {
  int len = domain.length();
  unsigned char* in = getUnsignedCharFromString(domain, len);

  unsigned char out[crypto_auth_hmacsha256_BYTES];

  crypto_auth_hmacsha256(out, in, len, this->getKey());

  if (crypto_auth_hmacsha256_verify(out, in, len, this->getKey()) != 0) {
    qDebug() << "Error! HMAC failed!";
    return NULL;
  }

  QString outString = getStringFromUnsignedChar(out);

  return outString.toLocal8Bit();
}

unsigned char* SqrlIdentity::signMessage(QString message,
                                         unsigned char* privateKey,
                                         unsigned char* publicKey) {
  /*
   * Debugging
   */
  printf("public key ");
  for (unsigned int i = 0; i < crypto_sign_PUBLICKEYBYTES; ++i) {
    printf("%02x", (unsigned char)publicKey[i]);
  }
  printf("\n");

  printf("private key ");
  for (unsigned int i = 0; i < crypto_sign_SECRETKEYBYTES; ++i) {
    printf("%02x", (unsigned char)privateKey[i]);
  }
  printf("\n");
  /*
   * End debugging
   */

  unsigned char* actualMessage = (unsigned char*)message.toAscii().constData();
  unsigned char sig[crypto_sign_BYTES];

  crypto_sign_detached(sig, NULL, actualMessage, message.length(), privateKey);

  if (crypto_sign_verify_detached(sig, actualMessage, message.length(),
                                  publicKey) != 0) {
    qDebug() << "Signing failed!";
    return NULL;
  }

  printf("signature ");
  for (unsigned int i = 0; i < crypto_sign_BYTES; ++i) {
    printf("%02x", (unsigned char)sig[i]);
  }
  printf("\n");

  unsigned char* ret = sig;

  return ret;
}
