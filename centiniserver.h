#ifndef CENTINISERVER_H
#define CENTINISERVER_H

#include <QObject>
#include <QSettings>
#include <QSqlDatabase>
#include <QTcpServer>
#include <QWebSocketServer>

#include "callbackquery.h"
#include "asteriskmanager.h"
#include "user.h"

class CentiniServer : public QObject
{
	Q_OBJECT

public:
	explicit CentiniServer(QObject *parent = 0);

	void run();

private:
	enum Query {
		CheckUser
	};

	QSettings *settings;
	QSqlDatabase database;
	AsteriskManager *asterisk;
	QTcpServer *tcpServer;
	QWebSocketServer *webSocketServer;
	QHash<QString, QString> actionIds; // Action, ActionID
	QHash<QString, QString> sipPeers; // IP Address, Peer
	QHash<QString, QString> channels; // Channel, Peer
	QHash<QString, int> channelStates; // Channel, Channel State
	QHash<QString, QDateTime> channelLastCalls; // Channel, Last Call
	QHash<QString, QString> users; // IP Address, Username
	QHash<QString, User *> agents, supervisors, managers;

	void executeQuery(CallbackQuery *query);

	User *lookupUser(QString ipAddress);
	User::PhoneState phoneStateOf(int channelState);
	QString channelPeer(QString channel);
	QDateTime durationLastCall(QString duration);

	void actionLogin(User *user, QString username, QString passwordHash);
	void actionLogout(User *user);
	void actionDial(QString peer, QString number);
	void actionHangup(User *user, QString username);
	void actionSpy(User *user, QString target);
	void actionWhisper(User *user, QString target);
	void actionJoinQueue(QString peer, QString queue);
	void actionPauseQueue(QString peer, QString queue, bool paused);
	void actionLeaveQueue(QString peer, QString queue);

	void callbackLogin(CallbackQuery *query);

	QVariantMap populateUserInfo(User *user);
	void broadcastUserEvent(User *sender, QHash<QString, User *> *receivers, User::Event event, QVariantMap fields);
	void broadcastUserEvent(User *sender, User::Event event, QVariantMap fields);
	void enumerateUserList(User *receiver, QHash<QString, User *> *senders);

private slots:
	void openDatabaseConnection();
	void connectToAsterisk();

	void onDatabaseQueryFinished();

	void onAsteriskConnected(QString version);
	void onAsteriskDisconnected();
	void onAsteriskResponseSent(AsteriskManager::Response response, QVariantMap headers, QString actionID);
	void onAsteriskEventGenerated(AsteriskManager::Event event, QVariantMap headers);

	void onTcpServerNewConnection();
	void onWebSocketServerNewConnection();

	void onUserActionReceived(User::Action action, QVariantMap fields);
	void onUserDisconnected();
	void onUserQueueStateChanged(User::QueueState queueState);
	void onUserPhoneStateChanged(User::PhoneState phoneStateOf);
	void onUserPeerChanged(QString peer);
};

#endif // CENTINISERVER_H
