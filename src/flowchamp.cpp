#include "flowchamp.h"

FlowChamp::FlowChamp(int& argc, char** argv) : QApplication(argc, argv) {
  connect(dialogDR, &DetermineRoleDialog::DRPlayAsHost, this,
          &FlowChamp::DRPlayAsHost);
  connect(dialogDR, &DetermineRoleDialog::DRPlayAsGuest, this,
          &FlowChamp::DRPlayAsGuest);
  connect(dialogDR, &DetermineRoleDialog::DRQuitGame, this,
          &FlowChamp::DRQuitGame);
  dialogDR->show();

  connect(dialogCG, &CreateGame::CGGoToManageRoom, this,
          &FlowChamp::CGGoToManageRoom);
  connect(dialogCG, &CreateGame::CGQuitGame, this, &FlowChamp::CGQuitGame);

  connect(dialogMR, &ManageRoom::MRStartGameForAll, this,
          &FlowChamp::MRStartGameForAll);
  connect(dialogMR, &ManageRoom::MRQuitGame, this, &FlowChamp::MRQuitGame);
  connect(this, &FlowChamp::MRUpdatePlayerList, dialogMR,
          &ManageRoom::MRUpdatePlayerList);
  connect(this, &FlowChamp::MRPassHostInfo, dialogMR,
          &ManageRoom::MRPassHostInfo);

  connect(dialogJG, &JoinExistingGame::JGGoToWaitingToStart, this,
          &FlowChamp::JGGoToWaitingToStart);
  connect(dialogJG, &JoinExistingGame::JGQuitGame, this,
          &FlowChamp::JGQuitGame);

  connect(dialogWS, &WaitingToStart::WSQuitGame, this, &FlowChamp::WSQuitGame);

  connect(dialogGD, &GameDialog::GDSpacePressed, this,
          &FlowChamp::GDSpacePressed);
  connect(dialogGD, &GameDialog::GDEscPressed, this, &FlowChamp::GDEscPressed);
  connect(dialogGD, &GameDialog::GDQuitGame, this, &FlowChamp::GDQuitGame);

  // Networking Connects
  // Guest Connections
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
  // Host Connections
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
  playerList.addPlayer(player);
  playerNames.append(player->getName());
  emit this->MRUpdatePlayerList(playerNames);
}
void FlowChamp::removePlayer(PlayerModel* player) {
  playerNames.removeOne(player->getName());
  playerList.removePlayer(player);
  emit this->MRUpdatePlayerList(playerNames);
}

void FlowChamp::DRPlayAsHost(QString playerNameIn) {
  qDebug() << "In DRPlayAsHost()";
  playerName = playerNameIn;
  setRole(true);
  HostModel* hostPlayer = new HostModel(0, hostHandler->getTCPServer());
  playerList.addPlayer(hostPlayer);
  dialogDR->hide();
  dialogCG->show();
}

void FlowChamp::DRPlayAsGuest(QString playerNameIn) {
  setRole(false);
  qDebug() << "In DRPlayAsGuest()";
  playerName = playerNameIn;
  dialogDR->hide();
  dialogJG->show();
}

void FlowChamp::DRQuitGame() {
  qDebug() << "In DRQuitGame()";
  dialogDR->accept();
}

void FlowChamp::CGGoToManageRoom(QHostAddress addressIn, QString portIn,
                                 QString roomCodeIn) {
  qDebug() << "In CGGoToManageRoom()";
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
    /// @todo Throw an error with network information
    dialogCG->hide();
    dialogDR->show();
  }
}

void FlowChamp::CGQuitGame() {
  qDebug() << "In CGQuitGame()";
  dialogCG->hide();
  dialogDR->show();
}

void FlowChamp::MRStartGameForAll() {
  qDebug() << "In MRStartGameForAll()";
  NPWelcomeToRoom packet;
  // for (PlayerModel* temp : playerList.keys()) {
  for (int i = 1; i <= playerList.getMaxUID(); i++) {
    PlayerModel* temp = playerList.getPlayer(i);
    if (temp->getUID() > 0 && temp->getTCPSocket()) {
      emit this->hostSendWelcomeToRoom(packet, temp->getTCPSocket());
    }
  }
  dialogMR->hide();
  dialogGD->show();
}

void FlowChamp::MRQuitGame() {
  qDebug() << "In MRQuitGame()";
  dialogMR->hide();
  dialogDR->show();
}

void FlowChamp::JGGoToWaitingToStart(QHostAddress addressIn, QString portIn,
                                     QString roomCodeIn) {
  qDebug() << "In JGGoToWaitingToStart()";
  if (guestHandler->connectToHost(addressIn, portIn)) {
    NPProvideRoomCode packet;
    packet.setRoomCode(roomCodeIn);
    packet.setName(playerName);
    emit this->guestSendRoomCode(packet);
    dialogJG->hide();
  } else {
    /// @todo say what went wrong when joining game
    dialogJG->hide();
    dialogDR->show();
  }
}

void FlowChamp::JGQuitGame() {
  qDebug() << "In JGQuitGame()";
  dialogJG->hide();
  dialogDR->show();
}

void FlowChamp::WSStartClientGame() {
  qDebug() << "In WSStartClientGame()";
  /// @todo Send the packet to the client to have them start the game
}

void FlowChamp::WSQuitGame() {
  qDebug() << "In WSQuitGame()";
  guestHandler->disconnectFromHost();
  dialogWS->hide();
  dialogDR->show();
}

void FlowChamp::GDSpacePressed() {
  qDebug() << "In GDSpacePressed()";
  if (this->isHost()) {
    qDebug() << "GDSpacePressedLocal is Host";
  } else {
    qDebug() << "GDSpacePressedLocal is Guest";
    NPSpacePressed packet;
    packet.setUID(0);
    emit this->guestSendSpacePressed(packet);
  }
}

void FlowChamp::GDEscPressed() { qDebug() << "In GDEscPressed()"; }

void FlowChamp::GDQuitGame() {
  qDebug() << "In GDQuitGame()";
  dialogGD->hide();
  delete dialogGD;
  dialogGD = new GameDialog();
  dialogDR->show();
}

// Guest to Host
void FlowChamp::hostHandleRoomCode(NPProvideRoomCode packet,
                                   QTcpSocket* socket) {
  NPRoomCodeStatus statusPacket;
  static int uidCount = 1;
  if (packet.getRoomCode() == hostRoomCode) {
    statusPacket.setRoomCodeStatus(true);
    PlayerModel* newPlayer = new PlayerModel(uidCount++, socket);
    newPlayer->setName(packet.getName());
    addPlayer(newPlayer);
  } else {
    statusPacket.setRoomCodeStatus(false);
  }
  emit this->hostSendRoomCodeStatus(statusPacket, socket);
}

void FlowChamp::hostHandleTerminateMe(NPTerminateMe packet,
                                      QTcpSocket* socket) {
  removePlayer(playerList.getPlayer(socket));
}
void FlowChamp::hostHandleSpacePressed(NPSpacePressed packet,
                                       QTcpSocket* socket) {
  qDebug() << "Host: Space Pressed by "
           << playerList.getPlayer(socket)->getName();
}

void FlowChamp::hostHandleGuestTerminated(QTcpSocket* socket) {
  PlayerModel* tmpPlayer = playerList.getPlayer(socket);
  removePlayer(tmpPlayer);
}

// Host To Guest
void FlowChamp::guestHandleRoomCodeStatus(NPRoomCodeStatus packet) {
  if (packet.getRoomCodeStatus() == true) {
    PlayerModel* player = new PlayerModel(packet.getUID());
    playerList.addPlayer(player);
    dialogWS->show();
  } else {
    /// @todo throw an error if the room code is invalid
    error* throwError = new error;
    throwError->throwErrorMsg("ERROR: Invalid Room Code");
    throwError->exec();
    delete throwError;
    JGQuitGame();
  }
}

void FlowChamp::guestHandleWelcomeToRoom(NPWelcomeToRoom packet) {
  dialogWS->hide();
  dialogGD->show();
}

void FlowChamp::guestHandleInGameInfo(NPInGameInfo packet) {
  /// @todo Handle all the in game info stuff
}

void FlowChamp::guestHandleEndGameInfo(NPEndGameInfo packet) {
  /// @todo Handle all the end of game info stuff
}
void FlowChamp::guestHandleTCPDropOut() {
  this->activeWindow()->hide();
  dialogDR->show();
}