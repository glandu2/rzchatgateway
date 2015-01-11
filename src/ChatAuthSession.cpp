#include "ChatAuthSession.h"
#include "GameSession.h"
#include "EventLoop.h"

ChatAuthSession::ChatAuthSession(GameSession* gameSession, const std::string& ip, uint16_t port, const std::string& account, const std::string& password, int serverIdx, int delayTime, int ggRecoTime, AuthCipherMethod method)
	: ClientAuthSession(gameSession), gameSession(gameSession), ip(ip), port(port), account(account), password(password), serverIdx(serverIdx), method(method), delayTime(delayTime), ggRecoTime(ggRecoTime)
{
	uv_timer_init(EventLoop::getLoop(), &delayRecoTimer);
	uv_timer_init(EventLoop::getLoop(), &ggRecoTimer);
	delayRecoTimer.data = this;
	ggRecoTimer.data = this;
}

void ChatAuthSession::connect() {
	ClientAuthSession::connect(ip, port, account, password, method);
}

void ChatAuthSession::delayedConnect() {
	if(delayTime > 0) {
		info("Will connect to auth in %dms\n", delayTime);
		uv_timer_start(&delayRecoTimer, &onDelayRecoExpired, delayTime, 0);
	} else {
		connect();
	}
}

void ChatAuthSession::onDelayRecoExpired(uv_timer_t *timer) {
	ChatAuthSession* thisInstance = (ChatAuthSession*)timer->data;

	thisInstance->info("End of delay connect timer, connecting to auth now\n");
	thisInstance->connect();
}

void ChatAuthSession::onAuthDisconnected() {
	delayedConnect();
}

void ChatAuthSession::onAuthResult(TS_ResultCode result, const std::string& resultString) {
	if(result != TS_RESULT_SUCCESS) {
		error("%s: Auth failed result: %d (%s)\n", account.c_str(), result, resultString.empty() ? "no associated string" : resultString.c_str());
		abortSession();
	} else {
		info("Retrieving server list\n");
		retreiveServerList();
	}
}

void ChatAuthSession::onServerList(const std::vector<ServerInfo>& servers, uint16_t lastSelectedServerId) {
	bool serverFound = false;

	for(size_t i = 0; i < servers.size(); i++) {
		const ServerInfo& serverInfo = servers.at(i);

		if(serverInfo.serverId == serverIdx && !serverFound) {
			gameSession->setGameServerName(serverInfo.serverName);
			serverFound = true;
		}
	}

	if(!serverFound) {
		error("Server with index %d not found\n", serverIdx);
		info("Server list (last id: %d)\n", lastSelectedServerId);
		for(size_t i = 0; i < servers.size(); i++) {
			const ServerInfo& serverInfo = servers.at(i);
			info("%d: %20s at %16s:%d %d%% user ratio\n",
					serverInfo.serverId,
					serverInfo.serverName.c_str(),
					serverInfo.serverIp.c_str(),
					serverInfo.serverPort,
					serverInfo.userRatio);
		}
	}

	info("Connecting to GS with index %d\n", serverIdx);
	selectServer(serverIdx);

	if(ggRecoTime > 0) {
		debug("Starting GG timer: %ds\n", ggRecoTime);
		uv_timer_start(&ggRecoTimer, &onGGTimerExpired, ggRecoTime*1000, 0);
	}
}

void ChatAuthSession::onGameDisconnected() {
	delayedConnect();
}

void ChatAuthSession::onGGTimerExpired(uv_timer_t *timer) {
	ChatAuthSession* thisInstance = (ChatAuthSession*)timer->data;
	thisInstance->info("GG timeout, reconnecting\n");
	thisInstance->gameSession->abortSession();
}

void ChatAuthSession::onGameResult(TS_ResultCode result) {
	if(result != TS_RESULT_SUCCESS) {
		error("GS returned an error while authenticating: %d\n", result);
		abortSession();
	} else {
		info("Connected to GS\n");
	}
}