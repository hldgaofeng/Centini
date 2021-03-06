#include <QMetaEnum>
#include <QSqlQuery>
#include <QSqlError>

#include "user.h"

User::User(QObject *parent) :
	QObject(parent),
	sessionId(0),
	pauseId(0)
{
	timerId = startTimer(15000);
}

User::~User()
{
	if (!username_.isEmpty())
		finishSession();
}

void User::setUsername(QString username)
{
	username_ = username;

	if (!username.isEmpty()) {
		killTimer(timerId);

		populateGroups();
		startSession();
	}
}

QString User::username()
{
	return username_;
}

void User::setFullname(QString fullname)
{
	fullname_ = fullname;
}

QString User::fullname() const
{
	return fullname_;
}

void User::setLevel(User::Level level)
{
	level_ = level;
}

User::Level User::level() const
{
	return level_;
}

QStringList User::groups()
{
	return groups_;
}

void User::setPeer(QString peer)
{
	peer_ = peer;

	QVariantMap fields;
	fields["peer"] = peer;

	sendEvent(User::PeerChanged, fields);

	emit peerChanged(peer);
}

QString User::peer()
{
	return peer_;
}

void User::addQueue(QString queue)
{
	if (!queues_.contains(queue))
		queues_ << queue;
}

void User::removeQueue(QString queue)
{
	queues_.removeAll(queue);
}

void User::setQueues(QStringList queues)
{
	queues_ = queues;
}

QStringList User::queues()
{
	return queues_;
}

void User::setQueueState(QString queue, User::QueueState queueState, QString pauseReason)
{
	queueState_ = queueState;

	QVariantMap fields;
	fields["username"] = username_;
	fields["queue"] = queue;
	fields["queue_state"] = queueStateText(queueState);

	if (!pauseReason.isEmpty())
		fields["pause_reason"] = pauseReason;

	sendEvent(User::QueueStateChanged, fields);

	emit queueStateChanged(queueState);
}

User::QueueState User::queueState() const
{
	return queueState_;
}

void User::setPauseReason(QString pauseReason)
{
	pauseReason_ = pauseReason;
}

QString User::pauseReason()
{
	return pauseReason_;
}

void User::setPhoneState(User::PhoneState phoneState)
{
	phoneState_ = phoneState;

	QVariantMap fields;
	fields["username"] = username_;
	fields["phone_state"] = phoneStateText(phoneState);

	if (!lastCall_.isNull())
		fields["duration"] = lastCall_.secsTo(QDateTime::currentDateTime());

	sendEvent(User::PhoneStateChanged, fields);

	emit phoneStateChanged(phoneState);
}

User::PhoneState User::phoneState()
{
	return phoneState_;
}

void User::setLastCall(QDateTime lastCall)
{
	lastCall_ = lastCall;

	qDebug() << "Username:" << username_ << "Last Call:" << lastCall;
}

QDateTime User::lastCall()
{
	return lastCall_;
}

void User::startPause()
{
    QSqlQuery query;
    query.prepare("INSERT INTO user_pause_log (username, start, reason) VALUES (:username, :start, :reason)");
    query.bindValue(":username", username_);
    query.bindValue(":start", QDateTime::currentDateTime());
	query.bindValue(":reason", pauseReason_);

	if (query.exec())
		pauseId = query.lastInsertId().toUInt();
	else
		qCritical() << "Pause start query error:" << query.lastError().text();
}

void User::finishPause()
{
    QSqlQuery query;
    query.prepare("UPDATE user_pause_log SET finish = :finish WHERE id = :id");
    query.bindValue(":finish", QDateTime::currentDateTime());
    query.bindValue(":id", pauseId);

    pauseId = 0;

    if (!query.exec())
		qCritical() << "Pause finish query error:" << query.lastError().text();
}

void User::retrievePause()
{
	QSqlQuery query;
	query.prepare("SELECT id, reason FROM user_pause_log WHERE username = :username AND finish IS NULL ORDER BY start DESC LIMIT 1");
	query.bindValue(":username", username_);

	if (query.exec()) {
		if (query.next()) {
			pauseId = query.value("id").toUInt();
			pauseReason_ = query.value("reason").toString();
		}
	}
}

void User::sendResponse(User::Action action, bool success, QVariantMap fields)
{
	fields["type"] = "Response";
	fields["response"] = enumText("Action", action);
	fields["success"] = success;

    sendMessage(fields);
}

void User::sendResponse(User::Request request, bool success, QVariantMap fields)
{
    fields["type"] = "Response";
    fields["request"] = enumText("Request", request);
    fields["success"] = success;

    sendMessage(fields);
}

void User::sendEvent(User::Event event, QVariantMap fields)
{
	fields["type"] = "Event";
	fields["event"] = enumText("Event", event);

	sendMessage(fields);
}

QString User::levelText(int index)
{
	return enumText("Level", index < 0 ? level_ : index);
}

QString User::phoneStateText(int index)
{
	return enumText("PhoneState", index < 0 ? phoneState_ : index);
}

QString User::queueStateText(int index)
{
	return enumText("QueueState", index < 0 ? queueState_ : index);
}

int User::levelIndex(QString text)
{
	return enumIndex("Level", text);
}

int User::actionIndex(QString text)
{
    return enumIndex("Action", text);
}

int User::requestIndex(QString text)
{
    return enumIndex("Request", text);
}

void User::timerEvent(QTimerEvent *event)
{
	disconnect();
}

bool User::parseMessageFields(QVariantMap fields)
{
	QString actionText = fields.take("action").toString(),
			requestText = fields.take("request").toString();

	if (!actionText.isEmpty())
		emit actionReceived((User::Action) actionIndex(actionText), fields);
	else if (!requestText.isEmpty())
		emit requestReceived((User::Request) requestIndex(requestText), fields);
	else
		return false;

	return true;
}

QString User::enumText(QString enumName, int index)
{
	const QMetaObject *object = metaObject();

	return object->enumerator(object->indexOfEnumerator(enumName.toLatin1().data())).valueToKey(index);
}

int User::enumIndex(QString enumName, QString text)
{
	const QMetaObject *object = metaObject();

	return object->enumerator(object->indexOfEnumerator(enumName.toLatin1().data())).keysToValue(text.toLatin1().data());
}

void User::startSession()
{
	QSqlQuery query;
	query.prepare("INSERT INTO user_session_log (username, start) VALUES (:username, :start)");
	query.bindValue(":username", username_);
	query.bindValue(":start", QDateTime::currentDateTime());

	if (query.exec())
		sessionId = query.lastInsertId().toUInt();
	else
		qCritical() << "Session start query error:" << query.lastError().text();
}

void User::finishSession()
{
	QSqlQuery query;
	query.prepare("UPDATE user_session_log SET finish = :finish WHERE id = :id");
	query.bindValue(":finish", QDateTime::currentDateTime());
	query.bindValue(":id", sessionId);

	if (!query.exec())
		qCritical() << "Session finish query error:" << query.lastError().text();
}

void User::populateGroups()
{
	QSqlQuery query;
    query.prepare("SELECT `group` FROM group_member WHERE username = :username");
	query.bindValue(":username", username_);

	if (query.exec()) {
		groups_.clear();

		while (query.next()) {
			QString group = query.value("group").toString();

			if (!group.isEmpty())
				groups_ << group;
		}
	} else {
		qCritical() << "Populate Group query error:" << query.lastError().text();
	}
}
