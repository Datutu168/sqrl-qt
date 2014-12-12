#ifndef SQRLIDENTITY_H
#define SQRLIDENTITY_H

#include <QUrl>
#include <sodium.h>
#include <QNetworkReply>

class SqrlIdentity : QObject {
  Q_OBJECT

 public:
  SqrlIdentity();
  bool createIdentity();
  bool loadIdentity();
  unsigned char* getKey();
  QString getHexKey();
  QByteArray makeDomainPrivateKey(QString domain);
  unsigned char* signMessage(QString message, unsigned char* privateKey,
                             unsigned char* publicKey);
  bool authenticate(QUrl url);

  public slots:
    void replyFinished(QNetworkReply* reply);

 private:
  unsigned char* key;
};

#endif
