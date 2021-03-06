#include <QString>
#include <QMap>
#include <QStringList>
#include <QNetworkReply>
#include <QDebug>
#include "sqrlauthenticator.h"
#include "sodiumwrap.h"

const QString CRLF = "\r\n";

// Transaction Information Flags
const int TIF_IDK_MATCH = 0x01;
const int TIF_PDK_MATCH = 0x02;
const int TIF_IP_MATCH = 0x04;
const int TIF_SQRL_DISABLED = 0x08;
const int TIF_TRANSIENT_FAILURE = 0x20;
const int TIF_COMMAND_FAILED = 0x40;
const int TIF_SQRL_FAILURE = 0x80;

SqrlAuthenticator::SqrlAuthenticator(SqrlIdentity* ident) {
  this->identity = ident;
  this->querySuccess = false;
}

QMap<QString,QString> SqrlAuthenticator::parseArgs(QString input) {
  QMap<QString,QString> output;
  QStringList list = input.split(CRLF,QString::SkipEmptyParts);

  for (int i = 0; i < list.size(); ++i) {
    QString line = list.at(i);

    // Split line based on first '='
    int j = line.indexOf('=');
    QString var = line.left(j);
    QString val = line.mid(j + 1);

    // Insert into map
    output.insert(var, val);
  }

  return output;
}

int SqrlAuthenticator::intToHex(int input) {
  int output = 0;

  while (input > 0) {
    int digit = input % 10;
    output += (digit * 16);
    input /= 10;
  }

  return output;
}

void SqrlAuthenticator::replyFinished(QNetworkReply* reply) {
  QVariant ret = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

  if (ret.isNull()) {
    qDebug() << "Error: null return type.";
    return;
  }

  int retCode = ret.toInt();
  switch (retCode) {
    case 200:
      qDebug() << "Successful HTTP request. Continuing.";
      break;
    case 301:
      qDebug() << "Illegal SQRL query or bad URL... (Response code 301)";
      qDebug() << "Body is: " << reply->readAll();
      return;
    case 404:
      qDebug() << "Page not found! (Response code 404)";
      return;
    default:
      qDebug() << "I don't know about response code" << retCode;
      return;
  }

  QString rawReply = reply->readAll();
  qDebug() << "raw reply:" << rawReply;
  this->lastResponse = rawReply;

  rawReply = this->unbase64url(rawReply);
  qDebug() << "decoded raw reply:" << rawReply;

  QMap<QString,QString> parsedReply = this->parseArgs(rawReply);
  qDebug() << "parsed reply:" << parsedReply;

  /*
   * Ensure server knows only about SQRL version 1.0.
   * Once there are multiple versions, will need to implement better parsing
   * here.
   */
  int ver = 0;
  if (parsedReply.contains("ver")) {
    ver = parsedReply.take("ver").toInt();
    if (ver != 1) {
      qDebug() << "Error: unknown SQRL version:" << parsedReply.value("ver");
      return;
    }
  }
  else {
    qDebug() << "Error: SQRL version not found!";
  }

  // Parse Transaction Information Flags (TIFs)
  if (parsedReply.contains("tif")) {
    bool success;
    QString rawTif = parsedReply.take("tif");
    qDebug() << "Raw TIF is" << rawTif;
    this->tif = rawTif.toInt(&success,16);
    qDebug() << "Parsed TIF is" << tif;
  }
  else {
    qDebug() << "Error: TIF not found!";
  }

  if ((tif & TIF_SQRL_FAILURE) != 0) {
    qDebug() << "SQRL failure (TIF 0x80). This is probably a bug.";
    if ((tif & TIF_COMMAND_FAILED) != 0)
      qDebug() << "Also got TIF_COMMAND_FAILED, as expected.";
    else
      qDebug() << "Didn't get TIF_COMMAND_FAILED. Server error?";
    //return;
  }
  else {
    if ((tif & TIF_COMMAND_FAILED) != 0)
      qDebug() << "Got unexpected TIF_COMMAND_FAILED.";
  }

  if ((tif & TIF_IDK_MATCH) != 0)
    qDebug() << "Got TIF_IDK_MATCH.";
  else
    this->createAccount = true;

  if ((tif & TIF_PDK_MATCH) != 0)
    qDebug() << "Got TIF_PDK_MATCH.";
  if ((tif & TIF_IP_MATCH) != 0)
    qDebug() << "Got TIF_IP_MATCH.";
  if ((tif & TIF_SQRL_DISABLED) != 0)
    qDebug() << "Got TIF_SQRL_DISABLED.";
  if ((tif & TIF_TRANSIENT_FAILURE) != 0)
    qDebug() << "Got TIF_TRANSIENT_FAILURE.";

  // Parse Server Friendly Name (SFN)
  if (parsedReply.contains("sfn")) {
    this->sfn = parsedReply.take("sfn");
    qDebug() << "Server is" << sfn;
  }
  else {
    qDebug() << "Error: Server name not found!";
  }

  QString nut = "";
  if (parsedReply.contains("nut")) {
    nut = parsedReply.take("nut");
    qDebug() << "Nut is" << nut;
  }
  else {
    qDebug() << "Error: Nut not found!";
  }

  if (parsedReply.contains("qry")) {
    this->qry = parsedReply.take("qry");
    qDebug() << "Qry is" << qry;
  }
  else {
    qDebug() << "Error: Qry not found!";
  }

  // We should be able to parse all arguments.
  if (!parsedReply.isEmpty())
    qDebug() << "Found some extra arguments:" << parsedReply;
  else
    qDebug() << "All arguments parsed!";

  querySuccess = true;
}

QString SqrlAuthenticator::getNextSqrlUrl() {
  // TODO: add error checking
  return "sqrl://" + this->domain + this->qry;
}

/*
 * Convert a string into it's Base64URL representation.
 * This is just like normal Base64, except '+' and '/' must be converted to
 * '-' and '_' respectively.
 */
QString SqrlAuthenticator::base64url(QString input) {
  QString base64 = input.toAscii().toBase64();
  base64.replace("+","-");
  base64.replace("/","_");
  return this->trim(base64);
}

QString SqrlAuthenticator::unbase64url(QString input) {
  return QString(QByteArray::fromBase64(input.toAscii()));
}

QString SqrlAuthenticator::trim(QString input) {
  QString out = input;
  qDebug() << "  pre-trim:" << out;
  while (out.endsWith("="))
    out.chop(1);
  qDebug() << "  post-trim:" << out;

  return out;
}

bool SqrlAuthenticator::sqrlCommand(QString command, QUrl url) {
  this->domain = url.host();

  QByteArray domainSeed = this->identity->makeDomainPrivateKey(this->domain);

  if (domainSeed.isNull()) {
    qDebug() << "Error: domain seed generation failed.";
    return false;
  }

  QByteArray privateKey = SodiumWrap::generatePrivateKey(domainSeed);
  QByteArray publicKey = SodiumWrap::ed25519PrivateKeyToPublicKey(privateKey);

  QString message = this->domain + url.path() + "?nut="
    + url.queryItemValue("nut");
  qDebug() << "message:" << message;

  QNetworkAccessManager* manager = new QNetworkAccessManager();
  QUrl post("https://" + message);
  QNetworkRequest request(post);
  qDebug() << "post:" << post;

  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    "application/x-www-form-urlencoded");
  request.setRawHeader("User-Agent","SQRL/1");

  QString publicKeyString(publicKey);
  qDebug() << "idk:";
  QString idk = publicKeyString;
  idk = this->base64url(idk);

  qDebug() << "private idk:";
  QString privateKeyString(privateKey);
  this->base64url(privateKeyString + publicKeyString);

  // Client arg
  QString client = "ver=1" + CRLF
    + "idk=" + idk + CRLF
    + "cmd=" + command + CRLF;

  if (this->createAccount) {
    QString ilk = this->base64url(this->identity->getIdentityLockKey());
    QString suk = ilk;
    suk.replace('A','Z');
    client = client
      + "suk=" + suk + CRLF
      + "vuk=" + ilk + CRLF;
  }

  qDebug() << "client string:";
  client = this->base64url(client);

  // Server arg
  qDebug() << "server string: ";
  QString server;
  if (this->lastResponse == "")
    server = this->base64url(url.toString());
  else
    server = this->lastResponse;

  message = client + server;
  qDebug() << "message:" << message;

  QByteArray signature = this->identity->signMessage(message, privateKey, publicKey);
  if (signature.isNull()) {
    qDebug() << "Error: signing failed!";
    return false;
  }

  qDebug() << "sig:";
  qDebug() << signature.toHex();
  QString sig(signature);
  sig = this->base64url(sig);

  QUrl params;
  params.addQueryItem("client",client);
  params.addQueryItem("server",server);
  params.addQueryItem("ids",sig);

  qDebug() << "params: " << params.encodedQuery();

  QObject::connect(manager, SIGNAL(finished(QNetworkReply*)), (QObject*)this,
                   SLOT(replyFinished(QNetworkReply*)));

  manager->post(request, params.encodedQuery());

  return false;
}

bool SqrlAuthenticator::ident(QUrl url) {
  this->sqrlCommand("ident", url);

  return false;
}

bool SqrlAuthenticator::query(QUrl url) {
  if (sodium_init() == -1) {
    qDebug() << "Error: sodium_init failed.";
    return false;
  }

  this->sqrlCommand("query", url);

  return false;
}
