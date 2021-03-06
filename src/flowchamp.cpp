#include "flowchamp.h"
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>

FlowChamp::FlowChamp(int& argc, char** argv) : QApplication(argc, argv) {
  connectDialogDR();
  dialogDR->show();

  connectDialogCG();
  connectDialogMR();
  connectDialogJG();
  connectDialogWS();
  connectDialogGD();
  connectGameInfoTimer();
  connectGuestHandler();
  connectHostHandler();
  initializeBalls();
  openSQL();
  globalHighScore = readHighScoreFromDatabase();
  globalHighScoreName = readHighScoreHolderFromDatabase();
}

FlowChamp::~FlowChamp() {
  delete dialogCG;
  delete dialogDR;
  delete dialogGD;
  delete dialogJG;
  delete dialogMR;
  delete dialogWS;

  delete hostHandler;
  delete guestHandler;
}

bool FlowChamp::isHost() { return isHostPlayer; }

void FlowChamp::setRole(bool isPlayerHost) { isHostPlayer = isPlayerHost; }

void FlowChamp::addPlayer(PlayerModel* player) {
  if (!player) {
    return;
  }
  playerList->addPlayer(player);
  playerNames.append(player->getName());
  emit this->MRUpdatePlayerList(playerNames);
}
void FlowChamp::removePlayer(PlayerModel* player) {
  if (!player) {
    return;
  }
  playerNames.removeOne(player->getName());
  playerList->removePlayer(player);
  emit this->MRUpdatePlayerList(playerNames);
}

void FlowChamp::makePlayerLunge(PlayerModel* player) {
  if (!player) {
    return;
  }

  if (player->spacePressed()) {
    int position = int(player->getPositionId());

    dialogGD->extendHead(position);
    bool* isColliding =
        dialogGD->determineCapturedBalls(player->getPositionId());
    for (int i = 0; i < 25; i++) {
      if (isColliding[i]) {
        hostBallInfo[i]->hideBall();
        player->increaseScore(1);
      }
    }
  }
}

void FlowChamp::hostTerminateGame() { reinitialize(); }

void FlowChamp::reinitialize() {
  if (dialogGD) {
    delete dialogGD;
  }
  gameStarted = false;
  dialogGD = new GameDialog;
  connectDialogGD();

  // handle balls
  for (int i = 0; i < 25; i++) {
    if (hostBallInfo[i]) delete hostBallInfo[i];
  }

  initializeBalls();
  stopGameTimer();
  stopSendInGameInfo();

  // handle Manage Room
  if (dialogMR) {
    dialogMR->disconnect();
    delete dialogMR;
  }
  dialogMR = new ManageRoom();
  connectDialogMR();

  if (playerList) {
    delete playerList;
    playerList = new PlayerList();
  }

  hostHandler->stopTCPServer();
  delete hostHandler;
  hostHandler = new HostNetworkHandler();
  connectHostHandler();
}

void FlowChamp::startSendInGameInfo() {
  if (!sendInGameInfoTimer) {
    return;
  }
  sendInGameInfoTimer->start(50);
}

void FlowChamp::stopSendInGameInfo() {
  if (!sendInGameInfoTimer) {
    return;
  }
  sendInGameInfoTimer->stop();
}

void FlowChamp::startGameTimer() {
  int gameTimeMilli = 60 * 1000;
  gameTimer->start(gameTimeMilli);
  connect(gameTimer, &QTimer::timeout, this,
          &FlowChamp::prepareAndSendEndGameInfo);
}

void FlowChamp::stopGameTimer() {
  if (gameTimer) {
    gameTimer->stop();
  }
}

void FlowChamp::initializeBalls() {
  for (int i = 0; i < 25; i++) {
    hostBallInfo[i] = new ball;
    qreal x = 400, y = 400, dx = 0, dy = 0;
    while (qSqrt(qPow(x, 2) + qPow(y, 2)) + 5 >= 300) {
      x = static_cast<qreal>(QRandomGenerator::global()->bounded(-300, 300));
      y = static_cast<qreal>(QRandomGenerator::global()->bounded(-300, 300));
    }

    while (dx == 0 && dy == 0) {
      dx = static_cast<qreal>(QRandomGenerator::global()->bounded(-10, 10));
      dy = static_cast<qreal>(QRandomGenerator::global()->bounded(-10, 10));
    }

    hostBallInfo[i]->initializeBall(x, y, dx, dy);
  }
}

int FlowChamp::openSQL() {
  db = QSqlDatabase::addDatabase("QSQLITE");
  QString path = "src/highScore.db";
  db.setDatabaseName(path);
  if (!db.open())
  {
    qDebug() << "Error: Unable to connect to MySQL database";
    return 1;
  } else 
  {
    qDebug() << "Successful connection to database";
    return 0;
  }
}

void FlowChamp::closeSQL() {
  if (db.isOpen())
  {
    db.close();
  }
}

qint32 FlowChamp::readHighScoreFromDatabase() { 
  if (!db.isOpen())
  {
    return static_cast<qint32>(0);
  }
  qint32 scorePulled;
  QSqlQuery q;
  q.prepare("SELECT score FROM scores");

  if ( q.exec() )
  {
    QSqlRecord rec = q.record();

    int scoreCol = rec.indexOf("score");
    while (q.next())
    {
      scorePulled = q.value(scoreCol).toInt();
    }
    qDebug() << "High Score: " << scorePulled;
    return static_cast<qint32>(scorePulled);
  }
  return static_cast<qint32>(0);
}

QString FlowChamp::readHighScoreHolderFromDatabase() {
  if (!db.isOpen())
  {
    return QString("");
  }
  QString namePulled;
  QSqlQuery q;
  q.prepare("SELECT name FROM scores");

  if ( q.exec() )
  {
    QSqlRecord rec = q.record();

    int nameCol = rec.indexOf("name");
    while (q.next())
    {
      namePulled = q.value(nameCol).toString();
    }
    qDebug() << "High Score Name: " << namePulled;
    return QString(namePulled);
  }
  
  return QString("Loser");
}

void FlowChamp::setNewHighScore(QString name, qint32 score) {
  if (!db.isOpen())
  {
    return;
  }

  QSqlQuery qDel;
  qDel.prepare("DELETE FROM scores");

  if ( qDel.exec() )
  {
    qDebug() << "Successfully deleted all previous scores"; // Successful delete from Database
  } else {
    return;
  }

  QSqlQuery qAdd;
  qAdd.prepare("INSERT INTO scores (score, name) VALUES (:score, :name)");
  qAdd.bindValue(":name", name);
  qAdd.bindValue(":score",score);
  if (qAdd.exec())
  {
    qDebug() << "Person added successfully";
  } else {
    qDebug() << "ERROR adding new High Score";
  }
}

void FlowChamp::connectGuestHandler() {
  if (!guestHandler) {
    guestHandler = new GuestNetworkHandler();
  } else {
    disconnect(guestHandler, &GuestNetworkHandler::recvRoomCodeStatus, this,
               &FlowChamp::guestHandleRoomCodeStatus);
    disconnect(guestHandler, &GuestNetworkHandler::recvWelcomeToRoom, this,
               &FlowChamp::guestHandleWelcomeToRoom);
    disconnect(guestHandler, &GuestNetworkHandler::recvInGameInfo, this,
               &FlowChamp::guestHandleInGameInfo);
    disconnect(guestHandler, &GuestNetworkHandler::recvEndGameInfo, this,
               &FlowChamp::guestHandleEndGameInfo);
    disconnect(guestHandler, &GuestNetworkHandler::tcpConnectionDropped, this,
               &FlowChamp::guestHandleTCPDropOut);
    disconnect(this, &FlowChamp::guestSendRoomCode, guestHandler,
               &GuestNetworkHandler::provideRoomCode);
    disconnect(this, &FlowChamp::guestSendTerminateMe, guestHandler,
               &GuestNetworkHandler::terminateMe);
    disconnect(this, &FlowChamp::guestSendSpacePressed, guestHandler,
               &GuestNetworkHandler::spacePressed);
  }
  connect(guestHandler, &GuestNetworkHandler::recvRoomCodeStatus, this,
          &FlowChamp::guestHandleRoomCodeStatus);
  connect(guestHandler, &GuestNetworkHandler::recvWelcomeToRoom, this,
          &FlowChamp::guestHandleWelcomeToRoom);
  connect(guestHandler, &GuestNetworkHandler::recvInGameInfo, this,
          &FlowChamp::guestHandleInGameInfo);
  connect(guestHandler, &GuestNetworkHandler::recvEndGameInfo, this,
          &FlowChamp::guestHandleEndGameInfo);
  connect(guestHandler, &GuestNetworkHandler::tcpConnectionDropped, this,
          &FlowChamp::guestHandleTCPDropOut);
  connect(this, &FlowChamp::guestSendRoomCode, guestHandler,
          &GuestNetworkHandler::provideRoomCode);
  connect(this, &FlowChamp::guestSendTerminateMe, guestHandler,
          &GuestNetworkHandler::terminateMe);
  connect(this, &FlowChamp::guestSendSpacePressed, guestHandler,
          &GuestNetworkHandler::spacePressed);
}

void FlowChamp::connectHostHandler() {
  if (!hostHandler) {
    hostHandler = new HostNetworkHandler();
  } else {
    hostHandler->disconnect();
  }

  connect(hostHandler, &HostNetworkHandler::provideRoomCode, this,
          &FlowChamp::hostHandleRoomCode);
  connect(hostHandler, &HostNetworkHandler::terminateMe, this,
          &FlowChamp::hostHandleTerminateMe);
  connect(hostHandler, &HostNetworkHandler::spacePressed, this,
          &FlowChamp::hostHandleSpacePressed);
  connect(hostHandler, &HostNetworkHandler::hostHandleGuestTerminated, this,
          &FlowChamp::hostHandleGuestTerminated);
  connect(this, &FlowChamp::hostSendRoomCodeStatus, hostHandler,
          &HostNetworkHandler::sendRoomCodeStatus);
  connect(this, &FlowChamp::hostSendWelcomeToRoom, hostHandler,
          &HostNetworkHandler::sendWelcomeToRoom);
  connect(this, &FlowChamp::hostSendInGameInfo, hostHandler,
          &HostNetworkHandler::sendInGameInfo);
  connect(this, &FlowChamp::hostSendEndGameInfo, hostHandler,
          &HostNetworkHandler::sendEndGameInfo);
}

void FlowChamp::connectDialogDR() {
  if (!dialogDR) {
    dialogDR = new DetermineRoleDialog();
  } else {
    dialogDR->disconnect();
  }

  connect(dialogDR, &DetermineRoleDialog::DRPlayAsHost, this,
          &FlowChamp::DRPlayAsHost);
  connect(dialogDR, &DetermineRoleDialog::DRPlayAsGuest, this,
          &FlowChamp::DRPlayAsGuest);
  connect(dialogDR, &DetermineRoleDialog::DRQuitGame, this,
          &FlowChamp::DRQuitGame);
}

void FlowChamp::connectDialogCG() {
  if (!dialogCG) {
    dialogCG = new CreateGame();
  } else {
    dialogCG->disconnect();
  }

  connect(dialogCG, &CreateGame::CGGoToManageRoom, this,
          &FlowChamp::CGGoToManageRoom);
  connect(dialogCG, &CreateGame::CGQuitGame, this, &FlowChamp::CGQuitGame);
}

void FlowChamp::connectDialogMR() {
  if (!dialogMR) {
    dialogMR = new ManageRoom();
  } else {
    dialogMR->disconnect();
  }
  connect(dialogMR, &ManageRoom::MRStartGameForAll, this,
          &FlowChamp::MRStartGameForAll);
  connect(dialogMR, &ManageRoom::MRQuitGame, this, &FlowChamp::MRQuitGame);
  connect(this, &FlowChamp::MRUpdatePlayerList, dialogMR,
          &ManageRoom::MRUpdatePlayerList);
  connect(this, &FlowChamp::MRPassHostInfo, dialogMR,
          &ManageRoom::MRPassHostInfo);
}

void FlowChamp::connectDialogJG() {
  if (!dialogJG) {
    dialogJG = new JoinExistingGame();
  } else {
    dialogJG->disconnect();
  }
  connect(dialogJG, &JoinExistingGame::JGGoToWaitingToStart, this,
          &FlowChamp::JGGoToWaitingToStart);
  connect(dialogJG, &JoinExistingGame::JGQuitGame, this,
          &FlowChamp::JGQuitGame);
}
void FlowChamp::connectDialogWS() {
  if (!dialogWS) {
    dialogWS = new WaitingToStart();
  } else {
    disconnect(dialogWS, &WaitingToStart::WSQuitGame, this,
               &FlowChamp::WSQuitGame);
  }

  connect(dialogWS, &WaitingToStart::WSQuitGame, this, &FlowChamp::WSQuitGame);
}
void FlowChamp::connectDialogGD() {
  if (!dialogGD) {
    dialogGD = new GameDialog();
  } else {
    dialogGD->disconnect();
  }

  connect(dialogGD, &GameDialog::GDSpacePressed, this,
          &FlowChamp::GDSpacePressed);
  connect(dialogGD, &GameDialog::GDEscPressed, this, &FlowChamp::GDEscPressed);
  connect(dialogGD, &GameDialog::GDQuitGame, this, &FlowChamp::GDQuitGame);
}

void FlowChamp::connectGameInfoTimer() {
  if (!sendInGameInfoTimer) {
    sendInGameInfoTimer = new QTimer();
  } else {
    sendInGameInfoTimer->disconnect();
  }
  connect(sendInGameInfoTimer, &QTimer::timeout, this,
          QOverload<>::of(&FlowChamp::prepareAndSendInGameInfo));
}

void FlowChamp::DRPlayAsHost(QString playerNameIn) {
  playerName = playerNameIn;
  setRole(true);
  HostModel* hostPlayer = new HostModel(1, hostHandler->getTCPServer());
  hostPlayer->setPositionId(static_cast<PlayerPosition>(1));
  playerList->addPlayer(hostPlayer);
  dialogDR->hide();
  dialogCG->show();
}

void FlowChamp::DRPlayAsGuest(QString playerNameIn) {
  setRole(false);
  playerName = playerNameIn;
  dialogDR->hide();
  dialogJG->show();
}

void FlowChamp::DRQuitGame() { dialogDR->accept(); }

void FlowChamp::CGGoToManageRoom(QHostAddress addressIn, QString portIn,
                                 QString roomCodeIn) {
  if (!hostHandler) {
    connectHostHandler();
  }

  if (hostHandler->startTCPServer(addressIn, portIn.toUShort())) {
    // Success
    QStringList newList;
    newList.append(playerName);
    hostRoomCode = roomCodeIn;
    playerNames = newList;
    emit this->MRUpdatePlayerList(newList);
    emit this->MRPassHostInfo(addressIn.toString(), portIn, roomCodeIn);
    dialogCG->hide();
    dialogMR->show();
  } else {
    error networkError;
    QString errorMessage = "Error setting up QTcpServer: " +
                           hostHandler->getTCPServer()->errorString();
    networkError.throwErrorMsg(errorMessage);
    dialogCG->hide();
    networkError.exec();
    dialogDR->show();
  }
}

void FlowChamp::CGQuitGame() {
  dialogCG->hide();
  dialogDR->show();
}

void FlowChamp::MRStartGameForAll() {
  NPWelcomeToRoom packet;
  static QString playerNames[6];
  playerList->getPlayer(1)->enableTimers();
  for (int i = 1; i <= playerList->getMaxUID(); i++) {
    PlayerModel* temp = playerList->getPlayer(i);
    if (temp && temp->getUID() > 0 && temp->getTCPSocket()) {
      playerNames[i] = temp->getName();
    }
  }
  playerNames[1] = playerName;
  packet.setNames(playerNames);
  for (int i = 1; i <= playerList->getMaxUID(); i++) {
    PlayerModel* temp = playerList->getPlayer(i);
    if (temp && temp->getUID() > 0 && temp->getTCPSocket()) {
      temp->enableTimers();
      emit this->hostSendWelcomeToRoom(packet, temp->getTCPSocket());
    }
  }
  gameStarted = true;
  dialogMR->hide();
  dialogGD->drawBoard();
  dialogGD->setNames(playerNames);
  // Spawn each player
  dialogGD->spawnAllPlayers();
  dialogGD->show();
  startSendInGameInfo();
  startGameTimer();
}

void FlowChamp::MRQuitGame() {
  stopSendInGameInfo();
  hostHandler->stopTCPServer();
  dialogMR->hide();
  dialogDR->show();
}

void FlowChamp::JGGoToWaitingToStart(QHostAddress addressIn, QString portIn,
                                     QString roomCodeIn) {
  if (!guestHandler) {
    return;
  }

  if (guestHandler->connectToHost(addressIn, portIn)) {
    NPProvideRoomCode packet;
    packet.setRoomCode(roomCodeIn);
    packet.setName(playerName);
    emit this->guestSendRoomCode(packet);
    dialogJG->hide();
  } else {
    error networkError;
    QString errorMessage = "Error connecting to Host: " +
                           guestHandler->getTcpSocket()->errorString();
    networkError.throwErrorMsg(errorMessage);
    dialogJG->hide();
    networkError.exec();
    guestHandler->disconnect();
    guestHandler->disconnectFromHost();
    delete guestHandler;
    guestHandler = new GuestNetworkHandler();
    connectGuestHandler();
    dialogDR->show();
  }
}

void FlowChamp::JGQuitGame() {
  dialogJG->hide();
  dialogDR->show();
}

void FlowChamp::WSStartClientGame() {
  dialogWS->hide();
  dialogGD->drawBoard();
  dialogGD->spawnAllPlayers();
  dialogGD->show();
}

void FlowChamp::WSQuitGame() {
  if (!guestHandler) {
    return;
  }

  guestHandler->disconnectFromHost();
  dialogWS->hide();
  dialogDR->show();
}

void FlowChamp::GDSpacePressed() {
  if (this->isHost()) {
    makePlayerLunge(playerList->getPlayer(1));
  } else {
    NPSpacePressed packet;
    packet.setUID(playerList->getMaxUID());
    emit this->guestSendSpacePressed(packet);
  }
}

void FlowChamp::GDEscPressed() { emit GDQuitGame(); }

void FlowChamp::GDQuitGame() {
  if (isHost()) {
    hostHandler->stopTCPServer();
    hostTerminateGame();
  } else {
    guestHandler->disconnectFromHost();
  }
  dialogGD->hide();
  dialogDR->show();
}

// Guest to Host
void FlowChamp::hostHandleRoomCode(NPProvideRoomCode packet,
                                   QTcpSocket* socket) {
  NPRoomCodeStatus statusPacket;
  int newPlayerID = playerList->getNewPlayerID();
  if ((packet.getRoomCode() == hostRoomCode) && (newPlayerID != -1) &&
      (gameStarted == false)) {
    statusPacket.setRoomCodeStatus(true);
    PlayerModel* newPlayer = new PlayerModel(newPlayerID, socket);
    newPlayer->setName(packet.getName());
    newPlayer->setPositionId(static_cast<PlayerPosition>(newPlayerID));
    addPlayer(newPlayer);
  } else {
    statusPacket.setRoomCodeStatus(false);
  }
  emit this->hostSendRoomCodeStatus(statusPacket, socket);
}

void FlowChamp::hostHandleTerminateMe(QTcpSocket* socket) {
  removePlayer(playerList->getPlayer(socket));
}
void FlowChamp::hostHandleSpacePressed(NPSpacePressed packet,
                                       QTcpSocket* socket) {
  makePlayerLunge(playerList->getPlayer(socket));
}

void FlowChamp::hostHandleGuestTerminated(QTcpSocket* socket) {
  PlayerModel* tmpPlayer = playerList->getPlayer(socket);
  removePlayer(tmpPlayer);
}

// Host To Guest
void FlowChamp::guestHandleRoomCodeStatus(NPRoomCodeStatus packet) {
  if (packet.getRoomCodeStatus() == true) {
    PlayerModel* player = new PlayerModel(packet.getUID());
    playerList->addPlayer(player);
    dialogWS->show();
  } else {
    error* throwError = new error;
    throwError->throwErrorMsg(
        "ERROR: COULD NOT JOIN GAME\n\nTroubleshooting:\n   Room code could be "
        "invalid\n   Room could be full\n   Game could already be in session");
    throwError->exec();
    delete throwError;
    JGQuitGame();
  }
}

void FlowChamp::guestHandleWelcomeToRoom(NPWelcomeToRoom packet) {
  static QString* names = packet.getNames();
  WSStartClientGame();
  dialogGD->setNames(names);
}

void FlowChamp::guestHandleInGameInfo(NPInGameInfo packet) {
  /// @note ball ranges from 0-24 while player ranges from 1-5 in UID.
  /// 0 Represents the High Score.
  qint32 xPos[25];
  qint32 yPos[25];
  static qint32 timeRemaining;
  timeRemaining = packet.getTimeRemaining();

  for (int i = 0; i < 25; i++) {
    xPos[i] = packet.getBallPosX(i);
    yPos[i] = packet.getBallPosY(i);
  }

  static bool previousExtensionStatus[6];
  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < 6; i++) {
      previousExtensionStatus[i] = false;
    }
    initialized = true;
  }
  static qint32 scores[6];
  for (int i = 0; i < 6; i++) {
    if (i != 0) {
      bool isextended = packet.isPlayerExtended(i);
      if (isextended != previousExtensionStatus[i]) {
        previousExtensionStatus[i] = isextended;
        if (!isextended) {
          dialogGD->retractHead(i);
        } else {
          dialogGD->extendHead(i);
        }
      }
    }
    scores[i] = packet.getPlayerScore(i);
  }
  dialogGD->updateInfo(scores, timeRemaining);
  dialogGD->setBallPos(xPos, yPos);
}

void FlowChamp::guestHandleEndGameInfo(NPEndGameInfo packet) {
  dialogGD->HandleInfoIn(packet.getHighScoreName(), packet.getHighScore(),
                         packet.getWinnerName(), packet.getWinnerScore());
}
void FlowChamp::guestHandleTCPDropOut() {
  dialogCG->hide();
  dialogDR->hide();
  dialogGD->hide();
  dialogJG->hide();
  dialogMR->hide();
  dialogWS->hide();

  dialogDR->show();
}

void FlowChamp::prepareAndSendInGameInfo() {
  qint32 xPos[25];
  qint32 yPos[25];
  static qint32 scores[6];
  static qint32 timeRemaining;
  static bool previousExtensionStatus[6];
  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < 6; i++) {
      previousExtensionStatus[i] = false;
    }
    initialized = true;
  }

  NPInGameInfo packet;
  timeRemaining = gameTimer->remainingTime() / 1000;
  packet.setTimeRemaining(timeRemaining);
  for (int i = 0; i < 25; i++) {
    hostBallInfo[i]->advanceBall();
    qreal x = hostBallInfo[i]->pos().x();
    qreal y = hostBallInfo[i]->pos().y();
    packet.setBallPosition(i, x, y);
    xPos[i] = static_cast<qint32>(x);
    yPos[i] = static_cast<qint32>(y);
  }
  for (int i = 0; i < 6; i++) {
    if (i == 0) {
      packet.setPlayerScore(0, globalHighScore);
      scores[0] = globalHighScore;
      packet.setPlayerExtension(0, false);
    } else {
      PlayerModel* player = playerList->getPlayer(i);

      bool isextended = false;
      if (player) {
        scores[i] = player->getScore();

        isextended = player->isExtended();
        if (isextended != previousExtensionStatus[i]) {
          previousExtensionStatus[i] = isextended;
          if (!isextended) {
            dialogGD->retractHead(i);
          }
        }
      } else {
        scores[i] = 0;
      }
      packet.setPlayerExtension(i, isextended);
      packet.setPlayerScore(i, scores[i]);
    }
  }

  dialogGD->updateInfo(scores, timeRemaining);
  dialogGD->setBallPos(xPos, yPos);
  for (int i = 1; i <= playerList->getMaxUID(); i++) {
    PlayerModel* temp = playerList->getPlayer(i);
    if (temp && temp->getUID() > 0 && temp->getTCPSocket()) {
      emit this->hostSendInGameInfo(packet, temp->getTCPSocket());
    }
  }
}

void FlowChamp::prepareAndSendEndGameInfo() {
  stopGameTimer();
  for (int i = 1; i <= playerList->getMaxUID(); i++) {
    PlayerModel* temp = playerList->getPlayer(i);
    if (temp && temp->getUID() > 0) {
      temp->disableTimers();
    }
  }
  NPEndGameInfo packet;
  QString winnerName;
  qint32 winnerScore;
  winnerScore = 0;
  for (int i = 1; i <= playerList->getMaxUID(); i++) {
    PlayerModel* temp = playerList->getPlayer(i);
    if (temp && temp->getUID() > 0) {
      qint32 playerScorel = temp->getScore();
      QString playerNamel;
      if (i == 1) {
        playerNamel = playerName;
      } else {
        playerNamel = temp->getName();
      }

      if (playerScorel > winnerScore) {
        winnerScore = playerScorel;
        winnerName = playerNamel;
      }
      if (playerScorel > globalHighScore) {
        globalHighScore = playerScorel;
        globalHighScoreName = playerNamel;
        setNewHighScore(globalHighScoreName, globalHighScore);
      }
      closeSQL();
    }
  }
  packet.setWinnerInfo(winnerName, winnerScore);
  packet.setHighScoreInfo(globalHighScoreName, globalHighScore);
  dialogGD->HandleInfoIn(globalHighScoreName, globalHighScore, winnerName,
                         winnerScore);
  for (int i = 1; i <= playerList->getMaxUID(); i++) {
    PlayerModel* temp = playerList->getPlayer(i);
    if (temp && temp->getUID() > 0 && temp->getTCPSocket()) {
      emit this->hostSendEndGameInfo(packet, temp->getTCPSocket());
    }
  }
}
